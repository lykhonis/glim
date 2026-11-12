#include <glim/paint/Context.h>
#include <glim/paint/Renderer.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

int main() {
  const int w = 32;
  const int h = 24;
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(w * h * 4), 0);
  glim::paint::Renderer renderer(rgba.data(), w, h);

  glim::paint::Context ctx;
  ctx.setSize({static_cast<float>(w), static_cast<float>(h)});
  ctx.beginFrame();
  ctx.setFillColor(0xff0000ff);
  ctx.fill(glim::Rect::fromSize({static_cast<float>(w), static_cast<float>(h)}));
  ctx.finish();
  renderer.draw(ctx.scene());

  if (rgba[0] < 200 || rgba[1] > 20 || rgba[2] > 20) {
    std::cerr << "installed glim::paint-cpu did not fill red\n";
    return EXIT_FAILURE;
  }
  std::cout << "glim package cpu ok\n";
  return EXIT_SUCCESS;
}
