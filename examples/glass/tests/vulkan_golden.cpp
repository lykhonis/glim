#include <glim/gpu/Device.h>
#include <glim/image/Image.h>
#include <glim/image/Png.h>
#include <glim/paint/Context.h>
#include <glim/paint/Renderer.h>

#include <cstdlib>
#include <iostream>

void recordGlass(glim::paint::Context&, glim::Vec2, float, glim::Rect = {}, bool = false);

int main(int argc, char** argv) {
    const char* goldenPath = argc > 1 ? argv[1] : "examples/glass/golden/glass.png";
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
    recordGlass(ctx, {720, 480}, 0.0f);
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
        }
        return EXIT_FAILURE;
    }
    std::cout << "vulkan_golden ok\n";
    return EXIT_SUCCESS;
}
