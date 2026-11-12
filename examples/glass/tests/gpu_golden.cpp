#include "Glass.h"
#include "GpuGolden.h"

// The CPU lens is the LensMath reference; the band covers per-backend
// SDF smoothing. Metal measured 139853 / 119 (2026-11-12).

int main(int argc, char** argv) {
  return glim::examples::runGpuGolden(
      argc,
      argv,
      "examples/glass/golden/glass.png",
      {.maxOverDelta = 210000, .maxChannelDelta = 180},
      [](glim::paint::Context& ctx, glim::Vec2 size) {
        recordGlass(ctx, size, 0.0f);
      }
  );
}
