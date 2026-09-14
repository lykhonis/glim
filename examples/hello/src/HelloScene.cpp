#include "HelloScene.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

std::uint32_t helloBadge(glim::paint::Context& context) {
    static std::uint32_t id = 0;
    if (id != 0) {
        return id;
    }
    constexpr int k = 32;
    std::vector<std::uint8_t> px(static_cast<std::size_t>(k * k * 4));
    for (int y = 0; y < k; ++y) {
        for (int x = 0; x < k; ++x) {
            const bool cell = ((x / 8) + (y / 8)) % 2 == 0;
            const std::uint8_t r = cell ? 0x2a : 0xf0;
            const std::uint8_t g = cell ? 0x9d : 0xd5;
            const std::uint8_t b = cell ? 0x8f : 0xa8;
            const std::size_t i = static_cast<std::size_t>((y * k + x) * 4);
            px[i + 0] = r;
            px[i + 1] = g;
            px[i + 2] = b;
            px[i + 3] = 0xff;
        }
    }
    id = context.addImage(k, k, px.data());
    return id;
}

}  // namespace

void recordHello(glim::paint::Context& context, glim::Vec2 size, float timeSeconds) {
    using glim::Mat4;
    using glim::Radius;
    using glim::Rect;
    using glim::paint::GroupParams;

    const std::uint32_t badge = helloBadge(context);

    context.setFillColor(0x243038ff);
    context.fill(Rect::fromSize(size));

    context.save();
    context.translate({100.f, 50.f});
    context.setFillColor(0xac6363ff);
    context.fillRounded(Rect::fromSize({400.f, 300.f}), Radius{28.f});

    GroupParams scroll;
    scroll.clip = Rect{{16.f, 204.f}, {368.f, 80.f}};
    context.pushGroup(scroll);
    const float scrollX = std::fmod(timeSeconds * 48.f, 200.f);
    context.translate({16.f - scrollX, 216.f});
    const std::uint32_t pills[] = {0xf4a261ff, 0xe9c46aff, 0x2a9d8fff, 0x264653ff,
                                  0xe76f51ff, 0x8ab17dff, 0x3d7ea6ff, 0xd4a373ff};
    for (int i = 0; i < 8; ++i) {
        context.setFillColor(pills[i]);
        context.fillRounded(Rect{{static_cast<float>(i) * 56.f, 0.f}, {48.f, 56.f}}, Radius{14.f});
    }
    context.popGroup();
    context.restore();

    const float bob = std::sin(timeSeconds * 1.4f) * 10.f;
    const float sideX = std::max(520.f, size.x - 220.f);
    context.save();
    context.translate({sideX, 70.f + bob});
    context.setFillColor(0x3d7ea6ff);
    context.fillRounded(Rect::fromSize({180.f, 200.f}), Radius{20.f});
    context.setFillColor(0xf0d5a8ff);
    context.strokeRect(Rect{{12.f, 12.f}, {156.f, 176.f}}, Radius{14.f}, 3.f);
    context.restore();

    const Rect card{{140.f, 80.f}, {96.f, 64.f}};
    context.setFillColor(0xf0d5a8ff);
    context.fillRounded(card, Radius{12.f});
    if (badge != 0) {
        context.blit(Rect{{156.f, 96.f}, {32.f, 32.f}}, badge);
    }
    context.setFillColor(0x2a9d8fff);
    context.strokeRect(card, Radius{12.f}, 3.f);

    GroupParams glass;
    glass.opacity = 0.42f;
    glass.bounds = Rect{{0, 0}, {size.x, 96.f}};
    glass.transform = Mat4::translate(0, size.y - 96.f);
    context.pushGroup(glass);
    context.setFillColor(0xe8eef4ff);
    context.fill(Rect::fromSize({size.x, 96.f}));
    context.setFillColor(0x2a9d8fff);
    context.fillRounded(Rect{{24.f, 24.f}, {160.f, 48.f}}, Radius{16.f});
    context.setFillColor(0x243038ff);
    context.strokeRect(Rect{{24.f, 24.f}, {160.f, 48.f}}, Radius{16.f}, 2.f);
    context.popGroup();
}
