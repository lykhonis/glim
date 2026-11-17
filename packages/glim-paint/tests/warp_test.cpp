// A perspective-tilted group composites as its projected quad, not its
// bounding box, on the scene and packet paths alike.

#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>
#include <glim/paint/Software.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

constexpr int kW = 400;
constexpr int kH = 300;

// A 200x100 plate centered at (200, 150), swung 45 degrees about Y.
void recordTilt(glim::paint::Context& ctx) {
  ctx.setSize({kW, kH});
  ctx.beginFrame();
  glim::paint::GroupParams p;
  p.transform = glim::Mat4::translate(200, 150) * glim::Mat4::perspective(400) *
                glim::Mat4::rotateY(3.14159265f / 4) * glim::Mat4::translate(-200, -150);
  ctx.pushGroup(p);
  ctx.setFillColor(0x2050e0ff);
  ctx.fill(glim::Rect{{100, 100}, {200, 100}});
  ctx.popGroup();
  ctx.finish();
}

// The canvas is opaque; the plate is the only blue.
bool filled(const std::vector<std::uint8_t>& px, int x, int y) {
  const std::size_t i = static_cast<std::size_t>((y * kW + x) * 4);
  return px[i + 2] > 128 && px[i] < 100;
}

void check(const std::vector<std::uint8_t>& px, const char* path) {
  std::cout << path << ": center " << filled(px, 200, 150) << " near-edge top "
            << filled(px, 120, 94) << " far-edge top " << filled(px, 245, 104) << '\n';
  expect(filled(px, 200, 150), "the pivot stays covered");
  // The near (left) edge grows toward the viewer: above the flat top.
  expect(filled(px, 120, 94), "near edge reaches above the flat rect");
  // The far (right) edge shrinks: its top corner area of the flat rect is empty,
  // which a bounding-box composite would have filled.
  expect(!filled(px, 245, 104), "far edge top is outside the trapezoid");
  expect(!filled(px, 290, 150), "far edge pulls in toward the pivot");
}

}  // namespace

int main() {
  glim::paint::Context ctx;
  recordTilt(ctx);
  std::vector<std::uint8_t> scene(static_cast<std::size_t>(kW * kH * 4), 0);
  glim::paint::rasterScene(ctx.scene(), kW, kH, scene.data());
  check(scene, "scene");

  const glim::paint::FramePacket packet = glim::paint::encode(ctx.scene(), 1.0f);
  std::vector<std::uint8_t> packed(static_cast<std::size_t>(kW * kH * 4), 0);
  glim::paint::rasterPacket(packet, kW, kH, packed.data());
  check(packed, "packet");

  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
