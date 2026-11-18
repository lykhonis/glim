// The GPU renderer composites a perspective-tilted group through the warp
// pipeline: same trapezoid as the software path, read back from a target.

#include <glim/paint/Context.h>
#include <glim/gpu/Device.h>
#include <glim/paint/Renderer.h>

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
  glim::gpu::DeviceCreateInfo info{};
#if defined(__APPLE__)
  info.backend = glim::gpu::Backend::Metal;
#else
  info.backend = glim::gpu::Backend::Vulkan;
#endif
  auto created = glim::gpu::Device::create(info);
  if (!created.ok()) {
    std::cerr << "headless GPU device failed: " << created.message() << '\n';
    return EXIT_FAILURE;
  }
  glim::gpu::Device device = std::move(created.value());
  glim::paint::Renderer renderer(device);
  glim::paint::Context ctx;
  recordTilt(ctx);
  auto target = device.createFrameTarget({kW, kH});
  expect(target.ok(), "frame target");
  if (!target.ok()) return EXIT_FAILURE;
  renderer.drawToTarget(ctx.scene(), target.value());
  std::vector<std::uint8_t> px(static_cast<std::size_t>(kW * kH * 4), 0);
  device.readPixels(target.value(), px.data(), px.size());
  check(px, "gpu");
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
