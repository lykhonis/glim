#include <iostream>

#include "Glass.h"
#include "glim/toolkit.h"
#include "glim/toolkit/host/Host.h"
#include "glim/toolkit/host/Probe.h"

namespace host = glim::toolkit::host;

#if defined(__ANDROID__)
extern "C" int glim_app_main() {
  int argc = 0;
  char** argv = nullptr;
#else
int main(int argc, char** argv) {
#endif
  const host::E2eFlags flags = host::parseE2e(argc, argv);
  if (flags.help) {
    std::cout << "usage: glim-glass [--drive script [--drive-trace "
                 "out.json]] [--startup | "
                 "--startup-encoded] [--probe script]\n";
    return 0;
  }
  if (flags.any()) {
    host::TestApp t;
    t.w = 1024;
    t.h = 640;
    t.build = [](const glim::toolkit::Engine&) {
      return glim::toolkit::Block{};
    };
    t.paint = [](glim::paint::Context& ctx, glim::Vec2 size, float now
              ) { recordGlass(ctx, size, now); };
    return host::runE2e(t, flags);
  }
  // The stripes and lenses animate forever: a frame watch that never
  // settles.
  static glim::toolkit::Store::Watch watch;
  return host::run(
      {.title = "Glim Glass",
       .drive = flags.drive,
       .driveTrace = flags.driveTrace,
       .startup = flags.startup},
      [](const glim::toolkit::Engine& e) {
        if (!watch.live()) {
          glim::toolkit::State<float> tick =
              glim::toolkit::rootScope(e).state("tick", 0.0f);
          watch = e.store->get().watchFrame([tick](float now) mutable {
            tick = now;
            return true;
          });
        }
        return glim::toolkit::Block{};
      },
      [](host::Frame& f,
         const glim::toolkit::Block&,
         const glim::toolkit::Box&) {
        recordGlass(
            f.context, f.size, f.time, f.safeArea, f.reduceTransparency
        );
      }
  );
}
