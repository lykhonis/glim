#include <iostream>
#include <memory>

#include "Video.h"
#include "VideoSource.h"
#include "glim/toolkit.h"
#include "glim/toolkit/host/Host.h"
#include "glim/toolkit/host/Probe.h"

using namespace glim::toolkit;

#if defined(__ANDROID__)
extern "C" int glim_app_main() {
  int argc = 0;
  char** argv = nullptr;
#else
int main(int argc, char** argv) {
#endif
  const host::E2eFlags flags = host::parseE2e(argc, argv);
  if (flags.help) {
    std::cout << "usage: glim-video " << host::kE2eUsage << "\n";
    return 0;
  }
  if (flags.any()) {
    host::TestApp t;
    t.w = 960;
    t.h = 640;
    t.build = [](const Engine& e) {
      return videoTree({.playing = rootScope(e).state("playing", true)}
      );
    };
    return host::runE2e(t, flags);
  }
  // Playing keeps frames coming; paused, frames only follow input.
  static Store::Watch watch;
  static std::unique_ptr<VideoSource> source = makeVideoSource();
  return host::run(
    {
      .title = "Glim Video",
      .width = 960,
      .height = 640,
      .drive = flags.drive,
      .driveTrace = flags.driveTrace,
      .startup = flags.startup,
      .prepare =
        [](glim::paint::Context& ctx, Store&) {
          if (source) source->attach(ctx);
        },
    },
    [](const Engine& e) {
      Scope app = rootScope(e);
      VideoView view{.playing = app.state("playing", true)};
      if (source) {
        source->setPlaying(*view.playing);
        view.image = source->acquire();
      }
      if (!*view.playing || !source) {
        watch.reset();
      } else if (!watch.live()) {
        State<float> tick = app.state("tick", 0.0f);
        watch = e.store->get().watchFrame([tick](float now) mutable {
          tick = now;
          return true;
        });
      }
      return videoTree(std::move(view));
    },
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    // Periodic, not at exit: iOS never returns from run.
    [](const host::FrameTelemetry&) {
      static int frames = 0;
      if (!source || ++frames % 150 != 0) return;
      const host::FeedStats s = source->stats();
      std::cerr << "video feed published=" << s.published
                << " acquired=" << s.acquired
                << " fence_misses=" << s.fenceMisses
                << " dropped=" << s.dropped << " max_lag=" << s.maxLag
                << std::endl;
    }
  );
}
