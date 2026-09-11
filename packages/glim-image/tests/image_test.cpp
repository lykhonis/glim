#include <glim/image/Image.h>
#include <glim/image/Png.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

bool fileExists(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    std::fclose(f);
    return true;
}

}  // namespace

int main() {
    glim::image::Image a = glim::image::Image::rgba8(2, 2);
    expect(!a.empty() && a.byteCount() == 16, "rgba8 size");
    a.rgba[0] = 10;
    a.rgba[1] = 20;
    a.rgba[2] = 30;
    a.rgba[3] = 255;

    glim::image::Image same = a;
    expect(glim::image::compare(a, same).match(), "identical match");

    glim::image::Image off = a;
    off.rgba[0] = 13;
    const glim::image::Diff delta = glim::image::compare(a, off, 2);
    expect(delta.sameSize && delta.overDelta == 1 && !delta.match(), "channel over delta");
    expect(glim::image::compare(a, off, 3).match(), "within larger delta");

    glim::image::Image otherSize = glim::image::Image::rgba8(1, 1);
    const glim::image::Diff sized = glim::image::compare(a, otherSize);
    expect(!sized.sameSize && !sized.match(), "size mismatch");

    const char* path = "glim-image-test-roundtrip.png";
    std::remove(path);
    expect(glim::image::Png::write(path, a), "write png");
    glim::image::Image loaded;
    expect(glim::image::Png::read(path, &loaded), "read png");
    expect(glim::image::compare(a, loaded, 0).match(), "roundtrip exact");
    std::remove(path);

    expect(!glim::image::Png::read("glim-image-test-missing.png", &loaded), "missing file");

    const char* junk = "glim-image-test-junk.png";
    FILE* f = std::fopen(junk, "wb");
    expect(f != nullptr, "open junk");
    if (f) {
        const char bytes[] = "not a png";
        std::fwrite(bytes, 1, sizeof(bytes) - 1, f);
        std::fclose(f);
    }
    expect(fileExists(junk), "junk exists");
    expect(!glim::image::Png::read(junk, &loaded), "reject non-png");
    std::remove(junk);

    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "image_test ok\n";
    return EXIT_SUCCESS;
}
