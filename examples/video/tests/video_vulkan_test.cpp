// N7b gate (Linux): the Vulkan producer renders the clip on its own
// device into dmabufs; Glim's headless device imports and samples them.
#include <glim/gpu/Device.h>
#include <glim/paint/Context.h>
#include <glim/paint/Renderer.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <vector>

#include "VideoClip.h"
#include "VideoSource.h"

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

constexpr int kWidth = 160;
constexpr int kHeight = 90;

std::vector<std::uint8_t> draw(
  glim::gpu::Device& device,
  glim::paint::Renderer& renderer,
  glim::paint::Context& ctx,
  std::uint32_t imageId
) {
  ctx.setSize({kWidth, kHeight});
  ctx.beginFrame();
  ctx.setFillColor(0x000000ffu);
  ctx.fill(glim::Rect::fromSize({kWidth, kHeight}));
  if (imageId != 0) {
    ctx.blit(
      glim::Rect::fromSize({kWidth, kHeight}),
      glim::paint::Matter::foreign(imageId)
    );
  }
  ctx.finish();
  std::vector<std::uint8_t> px(kWidth * kHeight * 4);
  auto target = device.createFrameTarget({kWidth, kHeight});
  if (!target.ok()) return px;
  renderer.drawToTarget(ctx.scene(), target.value());
  device.readPixels(target.value(), px.data(), px.size());
  return px;
}

double mean(const std::vector<std::uint8_t>& px) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < px.size(); i += 4)
    sum += px[i] + px[i + 1] + px[i + 2];
  return static_cast<double>(sum) /
         static_cast<double>(px.size() / 4 * 3);
}

std::uint32_t waitFrame(glim::toolkit::VideoSource& source) {
  for (int i = 0; i < 200; ++i) {
    if (const std::uint32_t id = source.acquire()) return id;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return 0;
}

}  // namespace

int main() {
  glim::gpu::DeviceCreateInfo info{};
  info.backend = glim::gpu::Backend::Vulkan;
  auto created = glim::gpu::Device::create(info);
  if (!created.ok()) {
    std::cerr << "headless Vulkan device failed: " << created.message()
              << '\n';
    return EXIT_FAILURE;
  }
  glim::gpu::Device device = std::move(created.value());
  auto source = glim::toolkit::makeVideoSource();
  if (!source) {
    std::cerr << "FAIL: Vulkan dmabuf producer unavailable on this "
                 "device (/dev/udmabuf "
              << (access("/dev/udmabuf", F_OK) == 0 ? "present"
                                                    : "absent")
              << ")\n";
    return EXIT_FAILURE;
  }
  glim::paint::Context ctx;
  glim::paint::Renderer renderer(device);
  source->attach(ctx);

  const std::uint32_t first = waitFrame(*source);
  expect(first != 0, "a dmabuf frame becomes ready");
  const std::vector<std::uint8_t> a =
    draw(device, renderer, ctx, first);
  expect(mean(a) > 40.0, "clip visible, not the dark plate");
  expect(
    renderer.stats().importedBytes > 0, "dmabuf imported, not uploaded"
  );
  expect(renderer.stats().textureBytes == 0, "no uploaded textures");

  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  const std::vector<std::uint8_t> b =
    draw(device, renderer, ctx, source->acquire());
  expect(a != b, "clip moves while playing");

  for (int i = 0; i < 30; ++i) {
    draw(device, renderer, ctx, source->acquire());
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
  const glim::toolkit::host::FeedStats s = source->stats();
  std::cerr << "published " << s.published << " acquired " << s.acquired
            << " fenceMisses " << s.fenceMisses << " maxLag "
            << s.maxLag << " reuseWaits " << s.reuseWaits << '\n';
  expect(s.published >= 10, "producer keeps publishing");
  expect(
    renderer.stats().importedBytes <= glim::toolkit::kClipRing *
                                        glim::toolkit::kClipWidth *
                                        glim::toolkit::kClipHeight * 4u,
    "each ring buffer imported once"
  );
  source.reset();

  expect(
    !device
       .importSurface(
         nullptr, 4, 4, glim::gpu::SampleFormat::Rgba8Unorm
       )
       .ok(),
    "null surface rejected"
  );
  glim::gpu::DmaBuf closed;
  expect(
    !device
       .importSurface(
         &closed, 4, 4, glim::gpu::SampleFormat::Rgba8Unorm
       )
       .ok(),
    "dmabuf without an fd rejected"
  );

  const std::uint64_t imported = renderer.stats().importedBytes;
  const std::vector<std::uint8_t> red(4 * 4 * 4, 0xff);
  const std::uint32_t uploaded = ctx.addImage(4, 4, red.data());
  draw(device, renderer, ctx, uploaded);
  expect(
    renderer.stats().textureBytes == red.size(), "uploaded image held"
  );
  ctx.releaseImage(uploaded);
  for (int i = 0; i < 3; ++i) draw(device, renderer, ctx, 0);
  expect(
    renderer.stats().textureBytes == 0,
    "released image freed after its frames"
  );
  expect(
    renderer.stats().importedBytes == imported,
    "imports untouched by the release"
  );
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
