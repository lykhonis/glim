#include "GpuGolden.h"

void recordHello(
    glim::paint::Context&, glim::Vec2, float, glim::Rect = {}
);

int main(int argc, char** argv) {
  return glim::examples::runGpuGolden(
      argc,
      argv,
      "examples/hello/golden/hello.png",
      {.maxOverDelta = 2000, .maxChannelDelta = 48},
      [](glim::paint::Context& ctx, glim::Vec2 size) {
        recordHello(ctx, size, 0.0f);
      }
  );
}
