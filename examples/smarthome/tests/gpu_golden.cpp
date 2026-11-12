#include "GpuGolden.h"
#include "Scene.h"

// Text, images and glass through the GPU backend against the CPU
// golden. Pinned 2026-11-12; tighten only with a measured
// justification.
int main(int argc, char** argv) {
  return glim::examples::runGpuGolden(
      argc,
      argv,
      "examples/smarthome/golden/smarthome.png",
      {.maxOverDelta = 320000, .maxChannelDelta = 180},
      [](glim::paint::Context& ctx, glim::Vec2) {
        glim::toolkit::Store store;
        glim::toolkit::host::LayerFeed feed;
        paintSmartHomeScene(ctx, feed, store);
      }
  );
}
