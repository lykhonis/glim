#include <glim/toolkit/host/Host.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <utility>

#if !GLIM_SOFTWARE
#include <glim/shell/Surface.h>
#endif
#include <glim/paint/Overlay.h>
#include <glim/paint/Renderer.h>
#include <glim/shell/Application.h>
#include <glim/shell/RunLoop.h>
#include <glim/toolkit/host/Paint.h>
#include <glim/toolkit/host/Seats.h>

namespace glim {
namespace toolkit {
namespace host {

std::optional<Key> mapKey(shell::Key key) {
  switch (key) {
    case shell::Key::DpadLeft:
      return Key::Left;
    case shell::Key::DpadRight:
      return Key::Right;
    case shell::Key::DpadUp:
      return Key::Up;
    case shell::Key::DpadDown:
      return Key::Down;
    case shell::Key::Enter:
    case shell::Key::DpadCenter:
      return Key::Activate;
    default:
      return std::nullopt;
  }
}

int run(
    const Config& config,
    Screen& screen,
    Store& store,
    const Engine& engine,
    Build build,
    Record record,
    SeatViewFactory makeSeatView
) {
  SeatViewFactory effective = makeSeatView != nullptr ? makeSeatView : &makeDefaultSeatView;
  shell::Application host;
  shell::Window window;
  window.setTitle(config.title ? config.title : "Glim");
  window.setSize(config.width, config.height);
  window.center();

#if !GLIM_SOFTWARE
  shell::Surface surface;
  surface.attach(window);
  surface.setVSync(true);

  auto created = gpu::Device::create(surface.deviceCreateInfo());
  if (!created.ok()) {
    return 1;
  }
  gpu::Device device = std::move(created.value());
  paint::Renderer renderer(device);
#endif
  paint::Context context;
#if GLIM_SOFTWARE
  std::unique_ptr<paint::Renderer> renderer;
#endif
  paint::Overlay overlay;
  overlay.setEnabled(config.overlay);
  const auto start = std::chrono::steady_clock::now();
  auto lastTick = start;
  bool reduceTransparency = false;

  std::unordered_map<std::uint32_t, glim::Rect> attached;
  // States owned by the host loop; the window owns its size.
  Vec2 paintedSize{0.0f, 0.0f};
  // Build engine: measuring env from the caller plus this run's Store,
  // so tree functions allocate scoped state where they render. Trees
  // rebuild per event/paint — place() is cheap; the compositor
  // (Screen dirty gate + Overlay snapshot) owns idle-skip, so no
  // retained tree cache to invalidate.
  Engine active = engine;
  active.store = &store;
  const auto buildFresh = [&](const Vec2& size) {
    Block tree = build(active);
    Box box = active.place(tree, {0, size.x, 0, size.y});
    return std::pair<Block, Box>(std::move(tree), box);
  };

  const auto frameAt = [&](float t) {
    return Frame{
        context,
        window,
        window.size(),
        window.safeArea(),
        reduceTransparency,
        t,
#if !GLIM_SOFTWARE
        device,
#endif
    };
  };

  const auto tickClock = [&] {
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - lastTick).count();
    lastTick = now;
    overlay.tick(dt);
    return now;
  };

  const auto paintBody = [&](const std::chrono::steady_clock::time_point& now) {
    const Vec2 size = window.size();
    auto [tree, box] = buildFresh(size);
    context.setSize(size);
    context.beginFrame();
    Frame frame = frameAt(std::chrono::duration<float>(now - start).count());
    if (record) {
      record(frame, tree, box);
    }
    if (overlay.enabled()) {
#if GLIM_SOFTWARE
      if (renderer) {
        overlay.setStats(renderer->stats());
      }
#else
      overlay.setStats(renderer.stats());
#endif
      // Native seats sit above the scene; keep the chip out from
      // under them. Previous frame's placements: one frame stale,
      // invisible for a debug readout.
      glim::Rect avoid[8];
      int avoidCount = 0;
      for (const auto& e : attached) {
        if (avoidCount >= 8) break;
        avoid[avoidCount++] = e.second;
      }
      overlay.setAvoid(avoidCount > 0 ? avoid : nullptr, avoidCount);
      overlay.record(context, window.safeArea());
    }
    context.finish();
    syncSeats(window, context.scene(), attached, effective);
#if GLIM_SOFTWARE
    const int w = std::max(1, static_cast<int>(size.x));
    const int h = std::max(1, static_cast<int>(size.y));
    std::uint8_t* pixels = window.mapSoftware(w, h);
    if (!renderer) {
      renderer = std::make_unique<paint::Renderer>(pixels, w, h);
    } else {
      renderer->setTarget(pixels, w, h);
    }
    renderer->draw(context.scene());
    window.presentSoftware();
#else
    const Vec2 drawable = window.drawableSize();
    // Round, never truncate: the layer size is integral (see
    // syncMetalDrawableSize), and a truncating cast would shrink it
    // by up to a pixel mid-flight, oscillating against the sync and
    // desyncing the viewport from the layout size.
    device.setDrawableSize(
        static_cast<int>(std::lround(drawable.x)), static_cast<int>(std::lround(drawable.y))
    );
    renderer.draw(context.scene());
#endif
    screen.painted(store);
  };

  const auto paint = [&] {
    const auto now = tickClock();
    paintBody(now);
  };

  window.setEventCallback([&](const shell::Event& e) {
    using T = shell::EventType;
    if (e.type() == T::WindowClosed) {
      shell::RunLoop().stop();
      return;
    }
    if (e.type() == T::KeyDown && e.key() == shell::Key::Escape) {
      reduceTransparency = !reduceTransparency;
      paint();
      return;
    }
    if (e.type() == T::PointerDown) {
      auto [tree, box] = buildFresh(window.size());
      if (screen.pointerDown(tree, box, e.x(), e.y(), e.pointerId(), active)) {
        paint();
        return;
      }
      overlay.toggleExpanded();
      paint();
      return;
    }
    if (e.type() == T::PointerUp) {
      auto [tree, box] = buildFresh(window.size());
      if (screen.pointerUp(tree, box, e.x(), e.y(), active)) {
        paint();
      }
      return;
    }
    if (e.type() == T::PointerMove) {
      const bool pressed = e.button() != shell::PointerButton::None;
      if (!pressed) return;
      auto [tree, box] = buildFresh(window.size());
      if (screen.pointerMove(tree, box, e.x(), e.y(), pressed, e.pointerId(), active)) {
        paint();
      }
      return;
    }
    if (e.type() == T::KeyDown) {
      if (auto key = mapKey(e.key())) {
        auto [tree, box] = buildFresh(window.size());
        if (screen.key(tree, box, *key, active)) {
          paint();
        }
        return;
      }
      if (e.key() == shell::Key::Enter) {
        overlay.toggleExpanded();
        paint();
      }
      return;
    }
    if (e.type() == T::WindowResized) {
      const Vec2 size = window.size();
      if (size.x == paintedSize.x && size.y == paintedSize.y) {
        return;
      }
      // Paint synchronously inside the tracking iteration and gate
      // the present on its commit (view toggles
      // presentsWithTransaction): paint, geometry, and seat frames
      // all land in the same commit by construction, so background
      // and seats cannot skew a frame apart. Only in-iteration
      // paints glue this way — tick paints fire outside iterations,
      // where a gated present chases a later commit and skews worse.
      const auto now = std::chrono::steady_clock::now();
      paintBody(now);
      paintedSize = window.size();
      return;
    }
    if (e.type() == T::Frame) {
      // Clock ticks every vsync (cheap); the scene only re-records
      // when Screen's revision moved or the overlay snapshot turned
      // over. Idle frames do no rebuild, no record, no encode. Resize
      // work lives in the sync path above; the tick must not repaint
      // it or the two sources interleave phases and judder.
      const auto now = tickClock();
      if (screen.needsPaint(store) || overlay.statsDirty()) {
        paintBody(now);
        paintedSize = window.size();
      }
    }
  });
  window.show();
  host.run();
  return 0;
}

int run(const Config& config, Build build, Record record, SeatViewFactory makeSeatView) {
  // Owned defaults: the loop borrows these, so locals outlive it.
  Store store;
  Screen screen;
  Engine engine = measureEngine();
  LayerFeed feed;
  Record effective = record;
  if (!effective) {
    effective = [&](Frame& frame, const Block& tree, const Box& box) {
      Engine e = measureEngine();
      e.store = &store;
      paintScreen(frame.context, tree, box, e, feed, screen.focus, screen.focusVisible);
    };
  }
  return run(config, screen, store, engine, build, effective, makeSeatView);
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
