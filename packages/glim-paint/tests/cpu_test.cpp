#include <glim/paint/Context.h>
#include <glim/paint/Renderer.h>

#include <algorithm>
#include <cstdint>
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

    std::fill(buf.begin(), buf.end(), 0);
    ctx.beginFrame();
    ctx.setFillColor(0x000000ff);
    ctx.fill(glim::Rect::fromSize({static_cast<float>(w), static_cast<float>(h)}));
    ctx.setFillColor(0xff0000ff);
    ctx.fillRounded(glim::Rect{{8.f, 8.f}, {32.f, 32.f}}, glim::Radius{16.f});
    ctx.finish();
    renderer.draw(ctx.scene());
    const std::size_t center = static_cast<std::size_t>((24 * w + 24) * 4);
    const std::size_t corner = static_cast<std::size_t>((8 * w + 8) * 4);
    if (buf[center] < 200 || buf[center + 1] > 20) {
        std::cerr << "rounded fill missed center\n";
        return EXIT_FAILURE;
    }
    if (buf[corner] > 40) {
        std::cerr << "rounded fill should leave corner empty\n";
        return EXIT_FAILURE;
    }

    std::fill(buf.begin(), buf.end(), 0);
    ctx.beginFrame();
    ctx.setFillColor(0x000000ff);
    ctx.fill(glim::Rect::fromSize({static_cast<float>(w), static_cast<float>(h)}));
    ctx.setFillColor(0x00ff00ff);
    ctx.strokeRect(glim::Rect{{16.f, 12.f}, {24.f, 20.f}}, 4.f);
    ctx.finish();
    renderer.draw(ctx.scene());
    const std::size_t strokeEdge = static_cast<std::size_t>((12 * w + 16) * 4);
    const std::size_t strokeHole = static_cast<std::size_t>((22 * w + 28) * 4);
    if (buf[strokeEdge + 1] < 150) {
        std::cerr << "stroke missed outline\n";
        return EXIT_FAILURE;
    }
    if (buf[strokeHole + 1] > 40) {
        std::cerr << "stroke filled the hole\n";
        return EXIT_FAILURE;
    }

    std::fill(buf.begin(), buf.end(), 0);
    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(glim::Rect::fromSize({static_cast<float>(w), static_cast<float>(h)}));
    glim::paint::GroupParams clip;
    clip.clip = glim::Rect{{8.f, 0.f}, {16.f, static_cast<float>(h)}};
    ctx.pushGroup(clip);
    ctx.setFillColor(0x0000ffff);
    ctx.fill(glim::Rect{{-20.f, 0.f}, {80.f, static_cast<float>(h)}});
    ctx.popGroup();
    ctx.finish();
    renderer.draw(ctx.scene());
    const std::size_t inside = static_cast<std::size_t>((8 * w + 12) * 4);
    const std::size_t outside = static_cast<std::size_t>((8 * w + 40) * 4);
    if (buf[inside + 2] < 200) {
        std::cerr << "clip window should show scrolled content\n";
        return EXIT_FAILURE;
    }
    if (buf[outside] < 200 || buf[outside + 2] > 40) {
        std::cerr << "clip must not cover parent background\n";
        return EXIT_FAILURE;
    }

    std::cout << "cpu_test ok\n";
    return EXIT_SUCCESS;
}
