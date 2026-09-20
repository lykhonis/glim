#include <glim/shell/Application.h>

namespace glim::shell {

Application::Application() = default;
Application::~Application() = default;

void Application::run() {
#if !defined(__EMSCRIPTEN__)
    // Host stub: tests drive dispatch() directly.
#endif
}

}  // namespace glim::shell
