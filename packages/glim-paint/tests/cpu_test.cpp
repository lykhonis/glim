#include <glim/paint/Context.h>
#include <glim/paint/Renderer.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

int main() {
    const int w = 64;
    const int h = 48;
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(w * h * 4), 0);
    glim::paint::Renderer renderer(buf.data(), w, h);

    glim::paint::Context ctx;
    ctx.setSize({static_cast<float>(w), static_cast<float>(h)});
    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(glim::Rect::fromSize({static_cast<float>(w), static_cast<float>(h)}));
    ctx.finish();
    renderer.draw(ctx.scene());

    if (buf[0] < 200 || buf[1] > 20 || buf[2] > 20) {
        std::cerr << "cpu draw did not fill red\n";
        return EXIT_FAILURE;
    }

    std::fill(buf.begin(), buf.end(), 0);
    ctx.beginFrame();
    ctx.setFillColor(0x0000ffff);
    ctx.fill(glim::Rect::fromSize({static_cast<float>(w), static_cast<float>(h)}));
    const std::uint8_t white[4] = {255, 255, 255, 255};
    const std::uint32_t id = ctx.addImage(1, 1, white);
    ctx.blit(glim::Rect::fromSize({8.f, 8.f}), id);
    ctx.finish();
    renderer.draw(ctx.scene());
    if (buf[0] < 200 || buf[1] < 200 || buf[2] < 200) {
        std::cerr << "cpu blit did not sample white\n";
        return EXIT_FAILURE;
    }
    const std::size_t far = static_cast<std::size_t>((20 * w + 20) * 4);
    if (buf[far] > 20 || buf[far + 1] > 20 || buf[far + 2] < 200) {
        std::cerr << "cpu blit overwrote outside dest\n";
        return EXIT_FAILURE;
    }
    std::cout << "cpu_test ok\n";
    return EXIT_SUCCESS;
}
