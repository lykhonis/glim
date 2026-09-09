#include <glim/paint/Context.h>
#include <glim/paint/Raster.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    const char* goldenPath = argc > 1 ? argv[1] : "tests/golden/hello.png";
    glim::paint::Context ctx;
    glim::paint::recordHello(ctx, {720, 480}, 0.0f);

    std::vector<std::uint8_t> got(static_cast<std::size_t>(720 * 480 * 4));
    glim::paint::raster(ctx.scene(), 720, 480, got.data());

    std::vector<std::uint8_t> want;
    int w = 0;
    int h = 0;
    if (!glim::paint::readRgbaPng(goldenPath, &w, &h, &want)) {
        if (!glim::paint::writeRgbaPng(goldenPath, 720, 480, got.data())) {
            std::cerr << "could not write golden " << goldenPath << '\n';
            return EXIT_FAILURE;
        }
        std::cout << "wrote golden " << goldenPath << '\n';
        return EXIT_SUCCESS;
    }
    if (w != 720 || h != 480 || want.size() != got.size()) {
        std::cerr << "golden size mismatch\n";
        return EXIT_FAILURE;
    }
    std::size_t diffs = 0;
    for (std::size_t i = 0; i < got.size(); ++i) {
        const int d = static_cast<int>(got[i]) - static_cast<int>(want[i]);
        if (d > 2 || d < -2) {
            ++diffs;
        }
    }
    if (diffs != 0) {
        std::cerr << "golden mismatch: " << diffs << " channels differ by >2\n";
        return EXIT_FAILURE;
    }
    std::cout << "golden_test ok\n";
    return EXIT_SUCCESS;
}
