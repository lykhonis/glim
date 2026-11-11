#include "GpuGolden.h"

#include <glim/gpu/Device.h>
#include <glim/image/Golden.h>
#include <glim/image/Image.h>
#include <glim/image/Png.h>
#include <glim/paint/Renderer.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace glim::examples {
namespace {

#if defined(GLIM_GOLDEN_METAL)
constexpr gpu::Backend kBackend = gpu::Backend::Metal;
constexpr const char* kName = "metal";
#else
constexpr gpu::Backend kBackend = gpu::Backend::Vulkan;
constexpr const char* kName = "vulkan";
#endif

constexpr int kWidth = 720;
constexpr int kHeight = 480;

int diagnose(const image::Image& got, const image::Image& want) {
  std::size_t buckets[4] = {};
  int maxDelta = 0;
  int shown = 0;
  const std::size_t n = got.rgba.size();
  for (std::size_t i = 0; i < n; ++i) {
    const int d = std::abs(
        static_cast<int>(got.rgba[i]) - static_cast<int>(want.rgba[i])
    );
    if (d <= 2) {
      continue;
    }
    maxDelta = std::max(maxDelta, d);
    ++buckets[d <= 4 ? 0 : d <= 8 ? 1 : d <= 16 ? 2 : 3];
    if (d > 8 && shown < 8) {
      const std::size_t px = i / 4;
      const int x =
          static_cast<int>(px % static_cast<std::size_t>(got.width));
      const int y =
          static_cast<int>(px / static_cast<std::size_t>(got.width));
      std::cerr << "  diff@" << x << "," << y << " ch" << (i % 4)
                << " got=" << static_cast<int>(got.rgba[i])
                << " want=" << static_cast<int>(want.rgba[i]) << '\n';
      ++shown;
    }
  }
  std::cerr << "  maxDelta=" << maxDelta << " d3-4=" << buckets[0]
            << " d5-8=" << buckets[1] << " d9-16=" << buckets[2]
            << " d17+=" << buckets[3] << '\n';
  return maxDelta;
}

}  // namespace

int runGpuGolden(
    int argc,
    char** argv,
    const char* defaultGolden,
    GpuBand band,
    const RecordScene& record
) {
  const char* goldenPath = argc > 1 ? argv[1] : defaultGolden;
  gpu::DeviceCreateInfo info{};
  info.backend = kBackend;
  auto created = gpu::Device::create(info);
  if (!created.ok()) {
    std::cerr << "headless device failed: " << created.message()
              << '\n';
    return EXIT_FAILURE;
  }
  gpu::Device device = std::move(created.value());
  if (!device.headless()) {
    std::cerr << "expected headless device\n";
    return EXIT_FAILURE;
  }
  auto target = device.createFrameTarget({kWidth, kHeight});
  if (!target.ok()) {
    std::cerr << "frame target failed: " << target.message() << '\n';
    return EXIT_FAILURE;
  }
  const Vec2 size{
      static_cast<float>(kWidth), static_cast<float>(kHeight)
  };
  paint::Context ctx;
  ctx.setSize(size);
  ctx.beginFrame();
  record(ctx, size);
  ctx.finish();

  paint::Renderer renderer(device);
  renderer.drawToTarget(ctx.scene(), target.value());

  image::Image got = image::Image::rgba8(kWidth, kHeight);
  if (!device.readPixels(
          target.value(), got.rgba.data(), got.rgba.size()
      )) {
    std::cerr << "readPixels failed\n";
    return EXIT_FAILURE;
  }
  image::Image want;
  if (!image::Png::read(goldenPath, &want)) {
    std::cerr << "could not read golden " << goldenPath
              << " (CPU golden is source of truth)\n";
    return EXIT_FAILURE;
  }
  const image::Diff diff = image::compare(got, want);
  const int maxDelta = diagnose(got, want);
  if (!diff.sameSize || diff.overDelta > band.maxOverDelta ||
      maxDelta > band.maxChannelDelta) {
    if (!diff.sameSize) {
      std::cerr << "golden size mismatch\n";
    } else {
      std::cerr << kName << " golden mismatch: " << diff.overDelta
                << " channels differ by >2 (limit " << band.maxOverDelta
                << "), maxDelta=" << maxDelta << " (limit "
                << band.maxChannelDelta << ")\n";
    }
    image::writeGot(argv[0], got);
    return EXIT_FAILURE;
  }
  std::cout << kName << "_golden ok (over=" << diff.overDelta
            << ", maxDelta=" << maxDelta << ")\n";
  return EXIT_SUCCESS;
}

}  // namespace glim::examples
