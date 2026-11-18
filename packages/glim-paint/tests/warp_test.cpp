// The software renderer composites the shared cases on the scene and
// packet paths alike.

#include <glim/paint/FramePacket.h>
#include <glim/paint/Software.h>

#include "warp_cases.h"

using namespace warp_cases;

int main() {
  for (const Case& c : kCases) {
    glim::paint::Context ctx;
    c.record(ctx);
    Pixels scene(static_cast<std::size_t>(kW * kH * 4), 0);
    glim::paint::rasterScene(ctx.scene(), kW, kH, scene.data());
    c.check(scene, "scene");

    const glim::paint::FramePacket packet = glim::paint::encode(ctx.scene(), 1.0f);
    Pixels packed(static_cast<std::size_t>(kW * kH * 4), 0);
    glim::paint::rasterPacket(packet, kW, kH, packed.data());
    c.check(packed, "packet");
  }
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
