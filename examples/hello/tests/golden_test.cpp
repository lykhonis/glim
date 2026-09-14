#include <glim/image/Image.h>
#include <glim/image/Png.h>
#include <glim/paint/Context.h>
#include <glim/paint/Software.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>

void recordHello(glim::paint::Context&, glim::Vec2, float, glim::Rect = {});

namespace {

bool fileExists(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    std::fclose(f);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const char* goldenPath = argc > 1 ? argv[1] : "examples/hello/golden/hello.png";
    glim::paint::Context ctx;
    ctx.setSize({720, 480});
    ctx.beginFrame();
    recordHello(ctx, {720, 480}, 0.0f);
    ctx.finish();

    glim::image::Image got = glim::image::Image::rgba8(720, 480);
    glim::paint::rasterScene(ctx.scene(), got.width, got.height, got.rgba.data());

    if (!fileExists(goldenPath)) {
        if (!glim::image::Png::write(goldenPath, got)) {
            std::cerr << "could not write golden " << goldenPath << '\n';
            return EXIT_FAILURE;
        }
        std::cout << "wrote golden " << goldenPath << '\n';
        return EXIT_SUCCESS;
    }

    glim::image::Image want;
    if (!glim::image::Png::read(goldenPath, &want)) {
        std::cerr << "could not read golden " << goldenPath << '\n';
        return EXIT_FAILURE;
    }

    const glim::image::Diff diff = glim::image::compare(got, want);
    if (!diff.match()) {
        if (!diff.sameSize) {
            std::cerr << "golden size mismatch\n";
        } else {
            std::cerr << "golden mismatch: " << diff.overDelta << " channels differ by >2\n";
        }
        return EXIT_FAILURE;
    }
    std::cout << "golden_test ok\n";
    return EXIT_SUCCESS;
}
