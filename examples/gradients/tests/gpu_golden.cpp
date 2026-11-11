#include "GpuGolden.h"

void recordGradients(
    glim::paint::Context&, glim::Vec2, float, glim::Rect = {}
);

int main(int argc, char** argv) {
  return glim::examples::runGpuGolden(
      argc,
      argv,
      "examples/gradients/golden/gradients.png",
      {.maxOverDelta = 4000, .maxChannelDelta = 48},
      [](glim::paint::Context& ctx, glim::Vec2 size) {
        recordGradients(ctx, size, 0.0f);
      }
  );
}
