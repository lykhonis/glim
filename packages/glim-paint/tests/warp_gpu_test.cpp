// The GPU renderer composites the shared cases through the warp pipeline,
// read back from a target: the same pixels as the software path.

#include <glim/gpu/Device.h>
#include <glim/paint/Renderer.h>

#include "warp_cases.h"

using namespace warp_cases;

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
  // Nested isolates hold a target while inner ones take more: a handed-out
  // target never moves.
  {
    glim::paint::Renderer::TargetPool pool;
    glim::gpu::FrameTarget* outer = pool.acquire(device, 64, 64);
    for (int i = 0; i < 20; ++i) pool.acquire(device, 64 + 64 * i, 64);
    expect(outer == &pool.slots_.front().ft, "an acquired target stays put as the pool grows");
  }
  for (const Case& c : kCases) {
    glim::paint::Context ctx;
    c.record(ctx);
    auto target = device.createFrameTarget({kW, kH});
    expect(target.ok(), "frame target");
    if (!target.ok()) return EXIT_FAILURE;
    renderer.drawToTarget(ctx.scene(), target.value());
    Pixels px(static_cast<std::size_t>(kW * kH * 4), 0);
    device.readPixels(target.value(), px.data(), px.size());
    c.check(px, "gpu");
  }
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
