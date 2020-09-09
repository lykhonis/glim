#include <glim/shell/macos/Application.h>
#include <glim/shell/macos/Window.h>
#include <glim/shell/macos/Surface.h>
#include <glim/shell/macos/RunLoop.h>
#include <glim/paint/gl/Context.h>

int main() {
    glim::shell::macos::Application app;
    glim::shell::macos::Window window;

    window.setEventCallback([](glim::shell::macos::Event event) {
        switch (event.type()) {
            case glim::shell::macos::Event::Type::WindowClosed:
                glim::shell::macos::RunLoop().stop();
                break;
            default:
                break;
        }
    });
    window.setTitle("Hello");
    window.setSize(720, 480);
    window.center();
    window.show();

    glim::shell::macos::Surface surface;
    surface.attachGlContext();
    surface.setIntervalSwap(false);
    surface.setOpaque(true);
    surface.attach(window.view());

    surface.makeCurrent();

    auto size = glim::paint::Size(400.0f, 300.0f);

    glim::paint::Context context;
    context.setSize({720.0f, 480.0f});
    context.setFillColor(0x334c4cff);
    context.fill(glim::paint::Size(720.0f, 480.0));
    context.translate({100.0f, 50.0f});
//    context.rotate(45.0f, {size.width() * 0.5f, size.height() * 0.5f});
    context.setFillColor(0xac6363ff);
    context.fill(size);

    surface.present();

    app.run();

    return 0;
}
