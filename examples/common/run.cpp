#include "run.h"

#include <algorithm>
#include <chrono>
#include <memory>

#if !GLIM_SOFTWARE
#include <glim/shell/Surface.h>
#endif
#include <glim/paint/Overlay.h>
#include <glim/paint/Renderer.h>
#include <glim/shell/Application.h>
#include <glim/shell/Event.h>
#include <glim/shell/RunLoop.h>

int glimRunExample(const ExampleApp& app, void (*record)(ExampleFrame&)) {
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
    glim::paint::Overlay overlay;
    overlay.setEnabled(true);
    const auto start = std::chrono::steady_clock::now();
    auto lastPaint = start;
    bool reduceTransparency = false;

    const auto paint = [&] {
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - lastPaint).count();
        lastPaint = now;
        const glim::Vec2 size = window.size();
        context.setSize(size);
        context.beginFrame();
        ExampleFrame frame{
            context,
            window,
            size,
            window.safeArea(),
            reduceTransparency,
            std::chrono::duration<float>(now - start).count(),
#if !GLIM_SOFTWARE
            device,
#endif
        };
        if (record) {
            record(frame);
        }
        if (overlay.enabled()) {
            overlay.tick(dt);
#if GLIM_SOFTWARE
            if (renderer) {
                overlay.setStats(renderer->stats());
            }
#else
            overlay.setStats(renderer.stats());
#endif
            overlay.record(context, window.safeArea());
        }
        context.finish();
#if GLIM_SOFTWARE
        const int w = std::max(1, static_cast<int>(size.x));
        const int h = std::max(1, static_cast<int>(size.y));
        std::uint8_t* pixels = window.mapSoftware(w, h);
        if (!renderer) {
            renderer = std::make_unique<glim::paint::Renderer>(pixels, w, h);
        } else {
            renderer->setTarget(pixels, w, h);
        }
        renderer->draw(context.scene());
        window.presentSoftware();
#else
        const glim::Vec2 drawable = window.drawableSize();
        device.setDrawableSize(static_cast<int>(drawable.x), static_cast<int>(drawable.y));
        renderer.draw(context.scene());
#endif
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
        if (e.type() == T::WindowResized || e.type() == T::Frame) {
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
