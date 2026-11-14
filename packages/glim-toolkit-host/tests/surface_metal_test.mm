#import <IOSurface/IOSurfaceRef.h>
#import <Metal/Metal.h>

#include <glim/gpu/Device.h>
#include <glim/paint/Context.h>
#include <glim/toolkit/host/Feed.h>
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

constexpr int kSide = 16;

IOSurfaceRef makeSurface(std::uint32_t bgra) {
  NSDictionary* props = @{
    (id)kIOSurfaceWidth : @(kSide),
    (id)kIOSurfaceHeight : @(kSide),
    (id)kIOSurfaceBytesPerElement : @4,
    (id)kIOSurfacePixelFormat : @(0x42475241),  // 'BGRA'
  };
  IOSurfaceRef s = IOSurfaceCreate((__bridge CFDictionaryRef)props);
  IOSurfaceLock(s, 0, nullptr);
  auto* base = static_cast<std::uint8_t*>(IOSurfaceGetBaseAddress(s));
  const std::size_t stride = IOSurfaceGetBytesPerRow(s);
  for (int y = 0; y < kSide; ++y) {
    auto* row = reinterpret_cast<std::uint32_t*>(base + y * stride);
    for (int x = 0; x < kSide; ++x) row[x] = bgra;
  }
  IOSurfaceUnlock(s, 0, nullptr);
  return s;
}

std::vector<std::uint8_t> draw(
  glim::gpu::Device& device, glim::paint::Renderer& renderer, glim::paint::Context& ctx,
  std::uint32_t imageId
) {
  ctx.setSize({kSide, kSide});
  ctx.beginFrame();
  ctx.setFillColor(0x000000ffu);
  ctx.fill(glim::Rect::fromSize({kSide, kSide}));
  if (imageId != 0) {
    ctx.blit(glim::Rect::fromSize({kSide, kSide}), glim::paint::Matter::foreign(imageId));
  }
  ctx.finish();
  auto target = device.createFrameTarget({kSide, kSide});
  std::vector<std::uint8_t> px(kSide * kSide * 4);
  if (!target.ok()) return px;
  renderer.drawToTarget(ctx.scene(), target.value());
  device.readPixels(target.value(), px.data(), px.size());
  return px;
}

bool near(const std::vector<std::uint8_t>& px, int r, int g, int b) {
  const std::size_t mid = (kSide / 2 * kSide + kSide / 2) * 4;
  return std::abs(px[mid] - r) <= 2 && std::abs(px[mid + 1] - g) <= 2 &&
         std::abs(px[mid + 2] - b) <= 2;
}

}  // namespace

int main() {
  @autoreleasepool {
    auto created = glim::gpu::Device::create({});
    if (!created.ok()) {
      std::cerr << "headless Metal device failed: " << created.message() << '\n';
      return EXIT_FAILURE;
    }
    glim::gpu::Device device = std::move(created.value());
    id<MTLDevice> mtl = (__bridge id<MTLDevice>)device.nativeDevice();
    id<MTLSharedEvent> event = [mtl newSharedEvent];

    IOSurfaceRef red = makeSurface(0xffff0000u);
    IOSurfaceRef green = makeSurface(0xff00ff00u);

    glim::paint::Context ctx;
    const std::uint32_t redId = ctx.wrapSurface(red, kSide, kSide);
    const std::uint32_t greenId = ctx.wrapSurface(green, kSide, kSide);
    expect(redId != 0 && greenId != 0, "wrapSurface ids");

    glim::toolkit::host::Feed feed([event] { return event.signaledValue; });
    glim::paint::Renderer renderer(device);

    feed.publish({redId, 1});
    expect(near(draw(device, renderer, ctx, feed.acquire()), 0, 0, 0), "fence 1 pending: no feed");
    expect(feed.stats().fenceMisses == 1, "pending fence counted as a miss");

    event.signaledValue = 1;
    expect(near(draw(device, renderer, ctx, feed.acquire()), 255, 0, 0), "IOSurface red sampled");

    feed.publish({greenId, 2});
    expect(near(draw(device, renderer, ctx, feed.acquire()), 255, 0, 0), "holds red while 2 pends");
    event.signaledValue = 2;
    expect(near(draw(device, renderer, ctx, feed.acquire()), 0, 255, 0), "IOSurface green sampled");

    const glim::paint::Stats mem = renderer.stats();
    expect(mem.importedBytes == 2u * kSide * kSide * 4u, "imported bytes: two surfaces");
    expect(mem.textureBytes == 0, "no uploaded textures");
    const glim::toolkit::host::FeedStats s = feed.stats();
    expect(s.published == 2 && s.fenceMisses == 2 && s.maxLag == 1, "feed stats");

    CFRelease(red);
    CFRelease(green);
  }
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
