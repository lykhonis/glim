#include <glim/gpu/Device.h>
#include <glim/paint/Context.h>
#include <glim/paint/Renderer.h>
#include <glim/shell/Application.h>
#include <glim/shell/Event.h>
#include <glim/shell/RunLoop.h>
#include <glim/shell/Surface.h>
#include <glim/shell/Window.h>

int main() {
    glim::shell::Application app;
    glim::shell::Window window;
    window.setTitle("Hello");
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
    context.setSize(window.size());

    window.setEventCallback([&](const glim::shell::Event& e) {
        using T = glim::shell::EventType;
        if (e.type() == T::WindowClosed) {
            glim::shell::RunLoop().stop();
            return;
        }
        if (e.type() == T::WindowResized) {
            context.setSize({static_cast<float>(e.width()), static_cast<float>(e.height())});
            return;
        }
        if (e.type() != T::Frame) {
            return;
        }

        context.beginFrame();
        context.setFillColor(0x334c4cff);
        context.fill(glim::Rect::fromSize(window.size()));
        context.save();
        context.translate({100.f, 50.f});
        context.setFillColor(0xac6363ff);
        context.fill(glim::Rect::fromSize({400.f, 300.f}));
        context.restore();
        context.finish();
        renderer.draw(context.scene());
    });
    window.show();
    app.run();
    return 0;
}
