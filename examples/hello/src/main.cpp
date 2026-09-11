#include <algorithm>
#include <chrono>
#include <memory>

#if !GLIM_SOFTWARE
#include <glim/gpu/Device.h>
#include <glim/shell/Surface.h>
#endif
#include <glim/paint/Context.h>
#include <glim/paint/Renderer.h>
#include <glim/shell/Application.h>
#include <glim/shell/Event.h>
#include <glim/shell/RunLoop.h>
#include <glim/shell/Window.h>

#include "HelloScene.h"

#if defined(__ANDROID__)
extern "C" int glim_app_main() {
#else
int main() {
#endif
    glim::shell::Application app;
    glim::shell::Window window;
    window.setTitle("Glim");
    window.setSize(720, 480);
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

    const auto paint = [&] {
        const glim::Vec2 size = window.size();
        const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
        recordHello(context, size, t);
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
        renderer.draw(context.scene());
#endif
    };

    window.setEventCallback([&](const glim::shell::Event& e) {
        using T = glim::shell::EventType;
        if (e.type() == T::WindowClosed) {
            glim::shell::RunLoop().stop();
            return;
        }
        if (e.type() == T::WindowResized || e.type() == T::Frame) {
            paint();
        }
    });
    window.show();
    app.run();
    return 0;
}
