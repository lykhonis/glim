#include "GpuGolden.h"

// Wide band: glass SDF smoothing legitimately differs per backend.
// Pinned 2026-11-03; tighten only with a measured justification
// (roadmap P0.7).
void recordGlass(
    glim::paint::Context&,
    glim::Vec2,
    float,
    glim::Rect = {},
    bool = false
);

int main(int argc, char** argv) {
  return glim::examples::runGpuGolden(
      argc,
      argv,
      "examples/glass/golden/glass.png",
      {.maxOverDelta = 280000, .maxChannelDelta = 224},
      [](glim::paint::Context& ctx, glim::Vec2 size) {
        recordGlass(ctx, size, 0.0f);
      }
  );
}
