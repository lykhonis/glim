#include <glim/toolkit/host/Host.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <utility>

#if !GLIM_SOFTWARE
#include <glim/shell/Surface.h>
#endif
#include <glim/paint/Renderer.h>
#include <glim/shell/Application.h>
#include <glim/shell/RunLoop.h>
#include <glim/toolkit/host/Drive.h>
#include <glim/toolkit/host/Paint.h>
#include <glim/toolkit/host/Seats.h>
#include <glim/toolkit/host/Startup.h>
#include <glim/toolkit/host/Telemetry.h>

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
    SeatViewFactory makeSeatView,
    A11ySink a11ySink,
    TelemetryHub* telemetry,
    TelemetrySink telemetrySink
) {
  std::unique_ptr<StartupReport> startup;
  if (config.startup) {
    startup = std::make_unique<StartupReport>();
    startup->mark("launch");
  }
  bool firstPainted = false;
  const auto presentedAt = std::make_shared<std::atomic<double>>(0.0);
  const auto startupMark = [&](const char* phase) {
    if (startup && !firstPainted) startup->mark(phase);
  };
  SeatViewFactory effective = makeSeatView != nullptr ? makeSeatView : &makeDefaultSeatView;
  LayerFeed feed;
  if (!record) {
    record = [&](Frame& frame, const Block& tree, const Box& box) {
      if (config.prepare) config.prepare(frame.context, store);
      Engine e = measureEngine();
      e.store = store;
      paintScreen(frame.context, tree, box, e, feed, screen.focus, screen.focusVisible);
    };
  }
  std::unique_ptr<DriveRun> drive;
  if (!config.drive.empty()) {
    drive = DriveRun::load(config.drive.c_str());
    if (!drive) {
      return 2;
    }
    // Tap paints through the telemetry seam; the app's sink still fires.
    telemetrySink = [d = drive.get(), app = std::move(telemetrySink)](const FrameTelemetry& t) {
      d->paint(t);
      if (app) {
        app(t);
      }
    };
  }
  shell::Application host;
  startupMark("application");
  shell::Window window;
  startupMark("window-new");
  window.setTitle(config.title ? config.title : "Glim");
  window.setSize(config.width, config.height);
  if (config.customTitlebar) {
    window.setCustomTitlebar(true);
  }
  window.center();
  startupMark("window");

#if !GLIM_SOFTWARE
  shell::Surface surface;
  surface.attach(window);
  surface.setVSync(true);

  auto created = gpu::Device::create(surface.deviceCreateInfo());
  if (!created.ok()) {
    return 1;
  }
  gpu::Device device = std::move(created.value());
  startupMark("device");
  paint::Renderer renderer(device);
  startupMark("renderer");
#endif
  paint::Context context;
#if GLIM_SOFTWARE
  std::unique_ptr<paint::Renderer> renderer;
#endif
  const auto start = std::chrono::steady_clock::now();
  auto lastTick = start;
  bool reduceTransparency = false;
  float lastFrameMs = 0.f;

  std::unordered_map<std::uint32_t, glim::Rect> attached;
  // States owned by the host loop; the window owns its size.
  Vec2 paintedSize{0.0f, 0.0f};
#if !GLIM_SOFTWARE
  // Last drawable size pushed to the GPU device. setDrawableSize
  // reallocates swapchain resources, so only push on actual change —
  // pushing every painted frame stalls nextDrawable (~1s hitches).
  int lastDrawableW = -1;
  int lastDrawableH = -1;
#endif
  // Build engine: measuring env plus this run's Store, so scoped state
  // allocates where it renders. No retained tree cache — the compositor
  // owns idle-skip, so trees rebuild per event/paint.
  Engine active = engine;
  active.store = store;
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
    screen.clock.advance(dt);
    lastFrameMs = dt * 1000.0f;
    return now;
  };

  // Single choke point for every layer: shell frameMs in, toolkit
  // build/record around user paint, paint/gpu stats after draw, one
  // snapshot fanned out to pull (hub) and push (sink).
  const auto paintBody = [&](const std::chrono::steady_clock::time_point& now, float frameMs) {
    startupMark("first-event");
    const Vec2 size = window.size();
    const bool wantTm = (telemetry != nullptr) || static_cast<bool>(telemetrySink);
    const auto b0 =
        wantTm ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    auto [tree, box] = buildFresh(size);
    startupMark("build");
    const auto b1 =
        wantTm ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    if (a11ySink) {
      a11ySink(a11yNodes(tree, box, active), screen.focus);
    }
    context.setSize(size);
    context.beginFrame();
    Frame frame = frameAt(std::chrono::duration<float>(now - start).count());
    const auto r0 =
        wantTm ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    if (record) {
      record(frame, tree, box);
    }
    startupMark("record");
    const auto r1 =
        wantTm ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    context.finish();
    // Zero-cost when no native seats: sawSlots bit avoids collectSlots walk.
    if (!attached.empty() || context.sawSlots()) {
      syncSeats(window, context.scene(), attached, effective);
    }
#if GLIM_SOFTWARE
    const Vec2 drawable = window.drawableSize();
    const int w = std::max(1, static_cast<int>(std::lround(drawable.x)));
    const int h = std::max(1, static_cast<int>(std::lround(drawable.y)));
    std::uint8_t* pixels = window.mapSoftware(w, h);
    if (!renderer) {
      renderer = std::make_unique<paint::Renderer>(pixels, w, h);
    } else {
      renderer->setTarget(pixels, w, h);
    }
    renderer->draw(context.scene());
    window.presentSoftware();
    if (startup && !firstPainted) *presentedAt = processUptimeMs();
#else
    const Vec2 drawable = window.drawableSize();
    // Round, never truncate: a truncating cast would shrink the integral
    // layer size by up to a pixel mid-flight and desync the viewport.
    // Guard: setDrawableSize reallocates, pushing every frame stalls nextDrawable.
    const int dw = static_cast<int>(std::lround(drawable.x));
    const int dh = static_cast<int>(std::lround(drawable.y));
    if (dw != lastDrawableW || dh != lastDrawableH) {
      device.setDrawableSize(dw, dh);
      lastDrawableW = dw;
      lastDrawableH = dh;
    }
    if (startup && !firstPainted) {
      device.notifyNextPresented([presentedAt] { *presentedAt = processUptimeMs(); });
    }
    renderer.draw(context.scene());
#endif
    if (telemetry != nullptr || telemetrySink) {
      paint::Stats stats{};
#if GLIM_SOFTWARE
      if (renderer) {
        stats = renderer->stats();
      }
#else
      stats = renderer.stats();
#endif
      FrameTelemetry sample;
      sample.frameMs = frameMs;
      sample.buildMs = std::chrono::duration<float>(b1 - b0).count() * 1000.0f;
      sample.recordMs = std::chrono::duration<float>(r1 - r0).count() * 1000.0f;
      sample.encodeMs = stats.encodeMs;
      sample.stats = stats;
      sample.size = size;
      sample.safeArea = window.safeArea();
      sample.time = std::chrono::duration<float>(now - start).count();
      sample.revision = store.revision();
      if (telemetry != nullptr) {
        telemetry->push(sample);
      }
      if (telemetrySink) {
        telemetrySink(sample);
      }
    }
    startupMark("encode");
    firstPainted = true;
    screen.painted(store);
  };

  // Input never paints inline: a burst of events between two vsyncs
  // coalesces into the next Frame's one paint, so queued input is not
  // stuck behind a GPU-bound paint per event.
  const auto invalidate = [&] { screen.dirty.invalidate(); };

  window.setEventCallback([&](const shell::Event& e) {
    using T = shell::EventType;
    if (e.type() == T::WindowClosed) {
      shell::RunLoop().stop();
      return;
    }
    if (e.type() == T::KeyDown && e.key() == shell::Key::Escape) {
      reduceTransparency = !reduceTransparency;
      invalidate();
      return;
    }
    if (e.type() == T::PointerDown) {
      auto [tree, box] = buildFresh(window.size());
      if (screen.pointerDown(tree, box, e.x(), e.y(), e.pointerId(), active)) {
        invalidate();
      } else if (config.customTitlebar && e.y() < config.titleDragHeight) {
        // Empty chrome drags: toolkit ignored the press inside the
        // custom header strip, so hand it to AppKit. Controls that
        // consumed the press (buttons, sliders, nav) never reach here.
        window.dragWindow();
      }
      return;
    }
    if (e.type() == T::PointerUp) {
      auto [tree, box] = buildFresh(window.size());
      if (screen.pointerUp(tree, box, e.x(), e.y(), active)) {
        invalidate();
      }
      return;
    }
    if (e.type() == T::PointerMove) {
      const bool pressed = e.button() != shell::PointerButton::None;
      if (!pressed) return;
      auto [tree, box] = buildFresh(window.size());
      if (screen.pointerMove(tree, box, e.x(), e.y(), pressed, e.pointerId(), active)) {
        invalidate();
      }
      return;
    }
    if (e.type() == T::Wheel) {
      tickClock();
      auto [tree, box] = buildFresh(window.size());
      if (screen.wheel(tree, box, e.x(), e.y(), e.dx(), e.dy(), active)) {
        invalidate();
      }
      return;
    }
    if (e.type() == T::KeyDown) {
      if (auto key = mapKey(e.key())) {
        auto [tree, box] = buildFresh(window.size());
        if (screen.key(tree, box, *key, active)) {
          invalidate();
        }
      }
      return;
    }
    if (e.type() == T::WindowResized) {
      const Vec2 size = window.size();
      if (size.x == paintedSize.x && size.y == paintedSize.y) {
        return;
      }
      // Paint synchronously inside the tracking iteration so paint, geometry,
      // and seat frames land in the same commit. Tick paints must not repaint
      // here or the two sources interleave phases and judder.
      const auto now = std::chrono::steady_clock::now();
      paintBody(now, 0.f);
      paintedSize = window.size();
      return;
    }
    if (e.type() == T::Frame) {
      if (startup && *presentedAt > 0.0) {
        startup->markAt("presented", presentedAt->exchange(-1.0));
        shell::RunLoop().stop();
        return;
      }
      // Clock ticks every vsync; the scene re-records only when Screen's
      // revision moved. Live values step first (tree-free); their writes
      // bump the revision and paint below.
      const auto now = tickClock();
      if (store.valuesLive()) {
        store.stepValues(screen.clock.now);
      }
      if (screen.moving()) {
        auto [tree, box] = buildFresh(window.size());
        screen.tick(tree, box, active);
      }
      if (screen.needsPaint(store)) {
        paintBody(now, lastFrameMs);
        paintedSize = window.size();
      }
    }
  });
  window.show();
  startupMark("show");
  if (drive) {
    drive->start(window);
  }
  host.run();
  if (startup) {
    std::cout << startup->summary();
  }
  if (drive && !drive->finish(config.driveTrace.empty() ? nullptr : config.driveTrace.c_str())) {
    return 1;
  }
  return 0;
}

int run(
    const Config& config,
    Build build,
    Record record,
    SeatViewFactory makeSeatView,
    A11ySink a11ySink,
    TelemetryHub* telemetry,
    TelemetrySink telemetrySink
) {
  // Owned defaults: the loop borrows these, so locals must outlive it.
  Store store;
  Screen screen;
  Engine engine = measureEngine();
  return run(
      config, screen, store, engine, build, record, makeSeatView, a11ySink, telemetry, telemetrySink
  );
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
