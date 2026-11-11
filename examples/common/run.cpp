#include "run.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>

#if !GLIM_SOFTWARE
#include <glim/shell/Surface.h>
#endif
#include <glim/paint/Renderer.h>
#include <glim/shell/Application.h>
#include <glim/shell/Event.h>
#include <glim/shell/RunLoop.h>

int glimRunExample(
    const ExampleApp& app, void (*record)(ExampleFrame&)
) {
  glim::shell::Application host;
  glim::shell::Window window;
  window.setTitle(app.title ? app.title : "Glim");
  window.setSize(app.width, app.height);
  window.center();

#if !GLIM_SOFTWARE
  glim::shell::Surface surface;
  surface.attach(window);
  surface.setVSync(true);

  auto created = glim::gpu::Device::create(surface.deviceCreateInfo());
  if (!created.ok()) {
    return 1;
  }
  glim::gpu::Device device = std::move(created.value());
  glim::paint::Renderer renderer(device);
#endif
  glim::paint::Context context;
#if GLIM_SOFTWARE
  std::unique_ptr<glim::paint::Renderer> renderer;
#endif
  const auto start = std::chrono::steady_clock::now();
  auto lastTick = start;
  bool reduceTransparency = false;

  const auto frameAt = [&](float t) {
    return ExampleFrame{
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
    const float dt =
        std::chrono::duration<float>(now - lastTick).count();
    lastTick = now;
    (void)dt;
    return now;
  };

  const auto paintBody =
      [&](const std::chrono::steady_clock::time_point& now) {
        const glim::Vec2 size = window.size();
        context.setSize(size);
        context.beginFrame();
        ExampleFrame frame =
            frameAt(std::chrono::duration<float>(now - start).count());
        if (record) {
          record(frame);
        }
        context.finish();
#if GLIM_SOFTWARE
        // Device pixels: the layer shows the buffer at the backing
        // scale.
        const glim::Vec2 drawable = window.drawableSize();
        const int w =
            std::max(1, static_cast<int>(std::lround(drawable.x)));
        const int h =
            std::max(1, static_cast<int>(std::lround(drawable.y)));
        std::uint8_t* pixels = window.mapSoftware(w, h);
        if (!renderer) {
          renderer =
              std::make_unique<glim::paint::Renderer>(pixels, w, h);
        } else {
          renderer->setTarget(pixels, w, h);
        }
        renderer->draw(context.scene());
        window.presentSoftware();
#else
        const glim::Vec2 drawable = window.drawableSize();
        device.setDrawableSize(
            static_cast<int>(drawable.x), static_cast<int>(drawable.y)
        );
        renderer.draw(context.scene());
#endif
      };

  const auto paint = [&] {
    const auto now = tickClock();
    paintBody(now);
  };

  window.setEventCallback([&](const glim::shell::Event& e) {
    using T = glim::shell::EventType;
    if (e.type() == T::WindowClosed) {
      glim::shell::RunLoop().stop();
      return;
    }
    if (e.type() == T::KeyDown && e.key() == glim::shell::Key::Escape) {
      reduceTransparency = !reduceTransparency;
      paint();
      return;
    }
    {
      const auto now = std::chrono::steady_clock::now();
      ExampleFrame frame =
          frameAt(std::chrono::duration<float>(now - start).count());
      if (onExampleEvent(e, frame)) {
        paint();
        return;
      }
    }
    if (e.type() == T::WindowResized) {
      paint();
    }
    if (e.type() == T::Frame) {
      // Legacy harness repaints every Frame; toolkit apps use the host
      // loop.
      paint();
    }
  });
  window.show();
  host.run();
  return 0;
}

#if defined(__ANDROID__)
extern "C" int glim_app_main() {
#else
int main() {
#endif
#ifndef GLIM_EXAMPLE_TITLE
#define GLIM_EXAMPLE_TITLE "Glim"
#endif
  return glimRunExample({GLIM_EXAMPLE_TITLE, 720, 480}, recordExample);
}
