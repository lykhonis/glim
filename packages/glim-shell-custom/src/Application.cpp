#include <glim/shell/Application.h>
#include <glim/shell/RunLoop.h>

namespace glim::shell {

Application::Application() = default;
Application::~Application() = default;

void Application::run() {
    // OEM owns the frame clock; tests and embedders drive Window::injectFrame().
    RunLoop loop;
    loop.run();
}

}  // namespace glim::shell
