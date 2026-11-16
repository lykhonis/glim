#import <AppKit/AppKit.h>

#include <glim/shell/Application.h>
#include <glim/shell/Window.h>

#include <cstdlib>
#include <iostream>
#include <string>

#include "Share.h"
#include "glim/toolkit/host/TestApp.h"

using namespace glim::toolkit;

namespace {

int failures = 0;

void expect(bool ok, const std::string& what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

int visibleWindows() {
  int n = 0;
  for (NSWindow* w in NSApp.windows) n += w.isVisible ? 1 : 0;
  return n;
}

void spin(double seconds) { CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false); }

// Polls under load instead of guessing how long AppKit needs.
template <class Pred>
bool spinUntil(Pred done) {
  for (int i = 0; i < 100 && !done(); ++i) spin(0.05);
  return done();
}

}  // namespace

// N6 gate: the sample's Share button presents the real system sheet and its
// result comes back to the app's state.
int main() {
  @autoreleasepool {
    glim::shell::Application application;
    glim::shell::Window window;
    window.setSize(480, 320);
    window.show();
    spin(0.2);

    int results = 0;
    const host::Presenter presenter = host::windowPresenter(window, [&results] { ++results; });
    const Env env{&presenter, &typeid(host::Presenter), std::nullopt};
    host::TestApp app;
    Engine e = app.engine();
    e.env = env;
    State<std::string> shared = rootScope(e).state("shared", std::string());
    const std::function<void()> share = shareAction(e, shared);

    const int before = visibleWindows();
    share();
    expect(spinUntil([&] { return visibleWindows() > before; }), "the system picker is on screen");
    expect(results == 0, "the sheet stays open until dismissed");
    share();
    expect(*shared == "A sheet is already open", "a second sheet waits: " + *shared);

    window.dismissPresented();
    expect(*shared == "Share cancelled", "dismissal returns cancelled: " + *shared);
    expect(results == 2, "each present answers exactly once");
    expect(spinUntil([&] { return visibleWindows() == before; }), "dismissal closes the picker");

    share();
    spinUntil([&] { return visibleWindows() > before; });
    window.dismissPresented();
    expect(*shared == "Share cancelled", "the sheet presents again after closing");
    expect(results == 3, "the reopened sheet answers once");

    host::TestApp offscreen;
    Engine bare = offscreen.engine();
    State<std::string> none = rootScope(bare).state("shared", std::string());
    shareAction(bare, none)();
    expect(*none == "Sharing is not available here", "no window answers unsupported");
  }
  if (failures == 0) std::cout << "share sheet: presented, busy, cancelled, reopened\n";
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
