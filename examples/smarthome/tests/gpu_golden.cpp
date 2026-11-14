#include "GpuGolden.h"
#include "Scene.h"

// Text, images and glass through the GPU backend against the CPU
// golden. Pinned 2026-11-12; tighten only with a measured
// justification. 2026-11-13: thinner glass shows the detailed photo
// through the frost, where the GPU and CPU blurs differ (417k
// measured); the rest of the frame matches.
int main(int argc, char** argv) {
  return glim::examples::runGpuGolden(
    argc,
    argv,
    "examples/smarthome/golden/smarthome.png",
    {.maxOverDelta = 440000, .maxChannelDelta = 180},
    [](glim::paint::Context& ctx, glim::Vec2) {
      glim::toolkit::Store store;
      glim::toolkit::host::LayerFeed feed;
      paintSmartHomeScene(ctx, feed, store);
    }
  );
}
