#include <glim/gpu/Device.h>
#include <glim/image/Image.h>
#include <glim/image/Png.h>
#include <glim/paint/Context.h>
#include <glim/paint/Renderer.h>

#include <cstdlib>
#include <iostream>

#include <algorithm>
#include <string>

void recordGradients(glim::paint::Context&, glim::Vec2, float, glim::Rect = {});

static void diagnose(const glim::image::Image& got, const glim::image::Image& want) {
    std::size_t buckets[4] = {};
    int maxDelta = 0;
    int shown = 0;
    const std::size_t n = got.rgba.size();
    for (std::size_t i = 0; i < n; ++i) {
        const int d = std::abs(static_cast<int>(got.rgba[i]) - static_cast<int>(want.rgba[i]));
        if (d <= 2) {
            continue;
        }
        maxDelta = std::max(maxDelta, d);
        ++buckets[d <= 4 ? 0 : d <= 8 ? 1 : d <= 16 ? 2 : 3];
        if (d > 8 && shown < 8) {
            const std::size_t px = i / 4;
            const int x = static_cast<int>(px % static_cast<std::size_t>(got.width));
            const int y = static_cast<int>(px / static_cast<std::size_t>(got.width));
            std::cerr << "  diff@" << x << "," << y << " ch" << (i % 4)
                      << " got=" << static_cast<int>(got.rgba[i])
                      << " want=" << static_cast<int>(want.rgba[i]) << '\n';
            ++shown;
        }
    }
    std::cerr << "  maxDelta=" << maxDelta << " d3-4=" << buckets[0] << " d5-8=" << buckets[1]
              << " d9-16=" << buckets[2] << " d17+=" << buckets[3] << '\n';
}

int main(int argc, char** argv) {
    const char* goldenPath = argc > 1 ? argv[1] : "examples/gradients/golden/gradients.png";
    glim::gpu::DeviceCreateInfo info{};
    info.backend = glim::gpu::Backend::Vulkan;
    auto created = glim::gpu::Device::create(info);
    if (!created.ok()) {
        std::cerr << "headless device failed: " << created.message() << '\n';
        return EXIT_FAILURE;
    }
    glim::gpu::Device device = std::move(created.value());
    if (!device.headless()) {
        std::cerr << "expected headless device\n";
        return EXIT_FAILURE;
    }
    auto target = device.createFrameTarget({720, 480});
    if (!target.ok()) {
        std::cerr << "frame target failed: " << target.message() << '\n';
        return EXIT_FAILURE;
    }
    glim::paint::Context ctx;
    ctx.setSize({720, 480});
    ctx.beginFrame();
    recordGradients(ctx, {720, 480}, 0.0f);
    ctx.finish();

    glim::paint::Renderer renderer(device);
    renderer.drawToTarget(ctx.scene(), target.value());

    glim::image::Image got = glim::image::Image::rgba8(720, 480);
    if (!device.readPixels(target.value(), got.rgba.data(), got.rgba.size())) {
        std::cerr << "readPixels failed\n";
        return EXIT_FAILURE;
    }
    glim::image::Image want;
    if (!glim::image::Png::read(goldenPath, &want)) {
        std::cerr << "could not read golden " << goldenPath << " (CPU golden is source of truth)\n";
        return EXIT_FAILURE;
    }
    const glim::image::Diff diff = glim::image::compare(got, want);
    if (!diff.match()) {
        if (!diff.sameSize) {
            std::cerr << "golden size mismatch\n";
        } else {
            std::cerr << "vulkan golden mismatch: " << diff.overDelta << " channels differ by >2\n";
            diagnose(got, want);
            if (const char* dir = std::getenv("GLIM_GOT_DIR")) {
                std::string name = (argc > 0 && argv[0]) ? argv[0] : "vulkan-golden";
                const std::size_t slash = name.find_last_of('/');
                if (slash != std::string::npos) {
                    name = name.substr(slash + 1);
                }
                glim::image::Png::write((std::string(dir) + "/" + name + "-got.png").c_str(), got);
            }
        }
        return EXIT_FAILURE;
    }
    std::cout << "vulkan_golden ok\n";
    return EXIT_SUCCESS;
}
