#include <glim/shell/Application.h>
#include <glim/shell/Event.h>
#include <glim/shell/RunLoop.h>
#include <glim/shell/Window.h>

#include "Display.h"
#include "WindowRegistry.h"

namespace glim::shell {

Application::Application() {
    detail::waylandConnect();
}

Application::~Application() = default;

void Application::run() {
    if (!detail::waylandConnect()) {
        return;
    }
    RunLoop runLoop;
    runLoop.scheduleFrameCallback([] {
        const Event frame(EventType::Frame);
        for (Window* window : detail::shownWindows()) {
            window->dispatch(frame);
        }
    });
    runLoop.run();
}

}  // namespace glim::shell
