#include <glim/shell/Application.h>
#include <glim/shell/Event.h>
#include <glim/shell/Window.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include "Shown.h"

namespace glim::shell {

namespace {

void frame() {
  const Event e(EventType::Frame);
  for (Window* window : detail::shownWindows()) window->dispatch(e);
}

}  // namespace

Application::Application() = default;
Application::~Application() = default;

// The browser drives frames. The loop never returns, so the caller's
// state (the toolkit host's locals) lives as long as the page.
void Application::run() {
#if defined(__EMSCRIPTEN__)
  emscripten_set_main_loop(frame, 0, true);
#else
  frame();
#endif
}

}  // namespace glim::shell
