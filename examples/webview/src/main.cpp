#include <iostream>
#include <memory>

#include "Web.h"
#include "WebPage.h"
#include "glim/toolkit.h"
#include "glim/toolkit/host/Host.h"
#include "glim/toolkit/host/Probe.h"

using namespace glim::toolkit;

namespace {

std::unique_ptr<WebPage>& page() {
  static std::unique_ptr<WebPage> p = makeWebPage();
  return p;
}

// The window's logical size as of the last frame; the page fills it.
glim::Vec2& viewport() {
  static glim::Vec2 size{960, 640};
  return size;
}

WebScreen screenFor(WebPage* p) {
  if (!p)
    return {.mode = "No web view on this platform", .size = viewport()};
  return {
    .mode = p->mode(),
    .size = viewport(),
    .slot = p->seatViews() != nullptr,
    .seatId = kWebSeat,
    .image = p->acquire(),
    .input = [p](const LayerInput& in) { return p->input(in); },
  };
}

}  // namespace

#if defined(__ANDROID__)
extern "C" int glim_app_main(int argc, char** argv) {
#else
int main(int argc, char** argv) {
#endif
  const host::E2eFlags flags = host::parseE2e(argc, argv);
  if (flags.help) {
    std::cout << "usage: glim-webview " << host::kE2eUsage << "\n";
    return 0;
  }
  if (flags.any()) {
    host::TestApp t;
    t.w = 960;
    t.h = 640;
    t.build = [&t](const Engine&) {
      return webTree({.mode = "Headless", .size = {t.w, t.h}});
    };
    return host::runE2e(t, flags);
  }
  // Texture pages draw on their own clock; a frame rebuilds only when
  // the page has a newer one, so a still page costs no repaints.
  static Store::Watch watch;
  WebPage* p = page().get();
  return host::run(
    {
      .title = "Glim WebView",
      .width = 960,
      .height = 640,
      .drive = flags.drive,
      .driveTrace = flags.driveTrace,
      .startup = flags.startup,
      .prepare =
        [p](glim::paint::Context& ctx, Store&) {
          viewport() = ctx.size();
          if (p) p->attach(ctx);
        },
    },
    [p](const Engine& e) {
      WebScreen s = screenFor(p);
      if (!s.slot && p && !watch.live()) {
        State<float> tick = rootScope(e).state("tick", 0.0f);
        watch = e.store->get().watchFrame([tick, p](float now) mutable {
          if (p->advance()) tick = now;
          return true;
        });
      }
      return webTree(std::move(s));
    },
    nullptr,
    p ? p->seatViews() : nullptr
  );
}
