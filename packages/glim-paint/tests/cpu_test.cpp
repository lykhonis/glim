#include <glim/paint/Context.h>
#include <glim/paint/Renderer.h>

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
    std::cout << "cpu_test ok\n";
    return EXIT_SUCCESS;
}
