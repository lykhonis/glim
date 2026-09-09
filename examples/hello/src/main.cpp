#include <chrono>

#include <glim/gpu/Device.h>
#include <glim/paint/Context.h>
#include <glim/paint/Renderer.h>
#include <glim/shell/Application.h>
#include <glim/shell/Event.h>
#include <glim/shell/RunLoop.h>
#include <glim/shell/Surface.h>
#include <glim/shell/Window.h>

#include "HelloScene.h"

int main() {
    glim::shell::Application app;
    glim::shell::Window window;
    window.setTitle("Glim");
    window.setSize(720, 480);
    window.center();

    glim::shell::Surface surface;
    surface.attach(window);
    surface.setVSync(true);

    auto created = glim::gpu::Device::create(surface.deviceCreateInfo());
    if (!created.ok()) {
        return 1;
    }
    glim::gpu::Device device = std::move(created.value());
    glim::paint::Renderer renderer(device);
    glim::paint::Context context;
    const auto start = std::chrono::steady_clock::now();

    const auto paint = [&] {
        const glim::Vec2 size = window.size();
        const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
        recordHello(context, size, t);
        renderer.draw(context.scene());
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
