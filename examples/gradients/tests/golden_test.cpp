#include <glim/image/Golden.h>
#include <glim/image/Image.h>
#include <glim/image/Png.h>
#include <glim/paint/Context.h>
#include <glim/paint/Software.h>

#include <cstdlib>
#include <iostream>

void recordGradients(
    glim::paint::Context&, glim::Vec2, float, glim::Rect = {}
);

int main(int argc, char** argv) {
  const char* goldenPath =
      argc > 1 ? argv[1] : "examples/gradients/golden/gradients.png";
  glim::paint::Context ctx;
  ctx.setSize({720, 480});
  ctx.beginFrame();
  recordGradients(ctx, {720, 480}, 0.0f);
  ctx.finish();

  glim::image::Image got = glim::image::Image::rgba8(720, 480);
  glim::paint::rasterScene(
      ctx.scene(), got.width, got.height, got.rgba.data()
  );

  if (!glim::image::matchCpuGolden(
          got, goldenPath, argv[0], "gradients"
      ))
    return EXIT_FAILURE;
  std::cout << "golden_test ok\n";
  return EXIT_SUCCESS;
}
