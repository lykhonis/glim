#include <glim/paint/Context.h>
#include <glim/paint/Renderer.h>

#include <algorithm>
#include <cmath>
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

    std::fill(buf.begin(), buf.end(), 0);
    ctx.beginFrame();
    for (int x = 0; x < w; ++x) {
        ctx.setFillColor((x / 8) % 2 == 0 ? 0xff0000ff : 0x0000ffff);
        ctx.fill(glim::Rect{{static_cast<float>(x), 0.f}, {1.f, static_cast<float>(h)}});
    }
    glim::paint::GroupParams frost;
    frost.backdropBlur = 8.f;
    frost.bounds = glim::Rect{{16.f, 8.f}, {32.f, 24.f}};
    ctx.pushGroup(frost);
    ctx.setFillColor(0xffffff40);
    ctx.fill(glim::Rect{{16.f, 8.f}, {32.f, 24.f}});
    ctx.popGroup();
    ctx.setFillColor(0x00ff00ff);
    ctx.fill(glim::Rect{{20.f, 12.f}, {8.f, 8.f}});
    ctx.finish();
    renderer.draw(ctx.scene());
    const std::size_t frostPx = static_cast<std::size_t>((20 * w + 24) * 4);
    if (buf[frostPx] < 20 || buf[frostPx + 2] < 20) {
        std::cerr << "backdrop should mix dest stripes, not a hard color\n";
        return EXIT_FAILURE;
    }
    if (std::abs(static_cast<int>(buf[frostPx]) - static_cast<int>(buf[frostPx + 2])) > 220) {
        std::cerr << "backdrop blur should not keep a pure stripe\n";
        return EXIT_FAILURE;
    }
    const std::size_t overPx = static_cast<std::size_t>((16 * w + 24) * 4);
    if (buf[overPx + 1] < 200) {
        std::cerr << "overlay after backdrop should paint on top\n";
        return EXIT_FAILURE;
    }

    std::fill(buf.begin(), buf.end(), 0);
    ctx.beginFrame();
    for (int x = 0; x < w; ++x) {
        ctx.setFillColor((x / 8) % 2 == 0 ? 0xff0000ff : 0x0000ffff);
        ctx.fill(glim::Rect{{static_cast<float>(x), 0.f}, {1.f, static_cast<float>(h)}});
    }
    glim::paint::GroupParams lens;
    lens.backdropBlur = 8.f;
    lens.backdropBend = 0.55f;
    lens.bounds = glim::Rect{{16.f, 8.f}, {32.f, 24.f}};
    lens.clipRadius = glim::Radius{6.f};
    ctx.pushGroup(lens);
    ctx.popGroup();
    ctx.setFillColor(0x00ff00ff);
    ctx.fill(glim::Rect{{20.f, 12.f}, {8.f, 8.f}});
    ctx.finish();
    renderer.draw(ctx.scene());
    const std::size_t lensC = static_cast<std::size_t>((20 * w + 32) * 4);
    if (buf[lensC + 3] < 180) {
        std::cerr << "lens center should stay opaque\n";
        return EXIT_FAILURE;
    }
    if (buf[lensC] < 20 || buf[lensC + 2] < 20) {
        std::cerr << "lens should still sample dest, not a solid plate\n";
        return EXIT_FAILURE;
    }
    const std::size_t farPx = static_cast<std::size_t>((2 * w + 2) * 4);
    if (buf[farPx] < 200 || buf[farPx + 1] > 40 || buf[farPx + 2] > 40) {
        std::cerr << "wallpaper outside the lens should stay a dest stripe\n";
        return EXIT_FAILURE;
    }

    std::fill(buf.begin(), buf.end(), 0);
    ctx.beginFrame();
    ctx.setFillColor(0xffffffff);
    ctx.fill(glim::Rect::fromSize({static_cast<float>(w), static_cast<float>(h)}));
    glim::paint::GroupParams blob;
    blob.backdropBend = 0.5f;
    blob.backdropMerge = 20.f;
    blob.bounds = glim::Rect{{8.f, 12.f}, {48.f, 16.f}};
    ctx.pushGroup(blob);
    ctx.fillRounded(glim::Rect{{8.f, 12.f}, {16.f, 16.f}}, glim::Radius{8.f});
    ctx.fillRounded(glim::Rect{{40.f, 12.f}, {16.f, 16.f}}, glim::Radius{8.f});
    ctx.popGroup();
    ctx.finish();
    renderer.draw(ctx.scene());
    const std::size_t neck = static_cast<std::size_t>((20 * w + 32) * 4);
    if (buf[neck + 3] < 40) {
        std::cerr << "smooth-min should fill the neck between pills\n";
        return EXIT_FAILURE;
    }
    const std::size_t lensOver = static_cast<std::size_t>((16 * w + 24) * 4);
    if (buf[lensOver + 1] < 200) {
        std::cerr << "overlay after lens should paint on top\n";
        return EXIT_FAILURE;
    }

    std::cout << "cpu_test ok\n";
    return EXIT_SUCCESS;
}
