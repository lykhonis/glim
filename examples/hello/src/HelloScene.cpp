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

float minDim(glim::Vec2 a) {
    return std::min(a.x, a.y);
}

void recordMainCard(glim::paint::Context& context, glim::Rect card, float timeSeconds,
                    std::uint32_t badge) {
    using glim::Radius;
    using glim::Rect;
    using glim::paint::GroupParams;

    const float w = card.size.x;
    const float h = card.size.y;
    const float m = minDim(card.size);
    const float radius = std::clamp(m * 0.08f, 16.f, 32.f);
    const float inner = std::clamp(m * 0.055f, 12.f, 22.f);

    context.save();
    context.translate(card.origin);
    context.setFillColor(0xac6363ff);
    context.fillRounded(Rect::fromSize(card.size), Radius{radius});

    const float badgeW = std::clamp(w * 0.24f, 72.f, 112.f);
    const float badgeH = std::clamp(h * 0.22f, 48.f, 72.f);
    const Rect badgeCard{{inner * 1.15f, inner}, {badgeW, badgeH}};
    context.setFillColor(0xf0d5a8ff);
    context.fillRounded(badgeCard, Radius{12.f});
    if (badge != 0) {
        const float icon = std::min(32.f, std::min(badgeW, badgeH) - 16.f);
        context.blit(Rect{{badgeCard.origin.x + (badgeW - icon) * 0.5f,
                           badgeCard.origin.y + (badgeH - icon) * 0.5f},
                          {icon, icon}},
                     badge);
    }
    context.setFillColor(0x2a9d8fff);
    context.strokeRect(badgeCard, Radius{12.f}, 3.f);

    const float clipH = std::clamp(h * 0.26f, 52.f, 88.f);
    const float clipY = h - inner - clipH;
    const float clipW = std::max(8.f, w - inner * 2.f);
    const float fontPx = std::clamp(m * 0.085f, 20.f, 36.f);
    const glim::paint::TextSize hello = context.measureText("hello", fontPx);
    const float textX = inner * 1.15f;
    const float gapTop = badgeCard.origin.y + badgeH + inner * 0.45f;
    const float gapBot = clipY - inner * 0.2f;
    float baseline = gapTop + hello.ascent;
    if (baseline + hello.descent > gapBot) {
        baseline = std::max(hello.ascent, gapBot - hello.descent);
    }
    context.setFillColor(0xf0d5a8ff);
    context.text({textX, baseline}, "hello", fontPx);

    GroupParams scroll;
    scroll.clip = Rect{{inner, clipY}, {clipW, clipH}};
    context.pushGroup(scroll);
    const float pillH = std::max(36.f, clipH - 16.f);
    const float pillW = pillH * 0.86f;
    const float pillGap = pillW * 0.18f;
    const float scrollX = std::fmod(timeSeconds * (pillW + 8.f), pillW * 4.f + 8.f);
    context.translate({inner - scrollX, clipY + (clipH - pillH) * 0.5f});
    const std::uint32_t pills[] = {0xf4a261ff, 0xe9c46aff, 0x2a9d8fff, 0x264653ff,
                                  0xe76f51ff, 0x8ab17dff, 0x3d7ea6ff, 0xd4a373ff};
    for (int i = 0; i < 8; ++i) {
        context.setFillColor(pills[i]);
        context.fillRounded(Rect{{static_cast<float>(i) * (pillW + pillGap), 0.f}, {pillW, pillH}},
                            Radius{pillH * 0.25f});
    }
    context.popGroup();
    context.restore();
}

void recordSideCard(glim::paint::Context& context, glim::Rect card) {
    using glim::Radius;
    using glim::Rect;

    const float m = minDim(card.size);
    const float radius = std::clamp(m * 0.1f, 16.f, 28.f);
    const float inset = std::clamp(m * 0.07f, 10.f, 16.f);
    const float strokeR = std::max(8.f, radius - 6.f);

    context.save();
    context.translate(card.origin);
    context.setFillColor(0x3d7ea6ff);
    context.fillRounded(Rect::fromSize(card.size), Radius{radius});
    context.setFillColor(0xf0d5a8ff);
    context.strokeRect(Rect{{inset, inset}, {card.size.x - inset * 2.f, card.size.y - inset * 2.f}},
                       Radius{strokeR}, 3.f);
    context.restore();
}

}  // namespace

void recordHello(glim::paint::Context& context, glim::Vec2 size, float timeSeconds,
                 glim::Rect safeArea) {
    using glim::Mat4;
    using glim::Radius;
    using glim::Rect;
    using glim::paint::GroupParams;

    if (size.x < 1.f || size.y < 1.f) {
        return;
    }
    if (safeArea.size.x <= 0.f || safeArea.size.y <= 0.f) {
        safeArea = Rect::fromSize(size);
    }

    const std::uint32_t badge = helloBadge(context);

    context.setFillColor(0x243038ff);
    context.fill(Rect::fromSize(size));

    const float minSide = minDim(safeArea.size);
    const float pad = std::clamp(minSide * 0.045f, 12.f, 28.f);
    const float gap = std::clamp(minSide * 0.03f, 10.f, 20.f);
    const float barH = std::clamp(safeArea.size.y * 0.14f, 72.f, 104.f);

    const float workX = safeArea.origin.x + pad;
    const float workY = safeArea.origin.y + pad;
    const float workW = std::max(1.f, safeArea.size.x - pad * 2.f);
    const float workH = std::max(1.f, safeArea.size.y - pad * 2.f - barH - gap);
    const bool portrait = safeArea.size.y > safeArea.size.x;

    Rect main;
    Rect side;
    if (portrait) {
        const float mainH = std::max(120.f, workH * 0.58f);
        main = {{workX, workY}, {workW, mainH}};
        const float sideH = std::max(96.f, workH - mainH - gap);
        side = {{workX, workY + mainH + gap}, {workW, sideH}};
    } else {
        const float mainW = std::max(160.f, workW * 0.62f);
        const float sideW = std::max(96.f, workW - mainW - gap);
        main = {{workX, workY}, {mainW, workH}};
        side = {{workX + mainW + gap, workY}, {sideW, workH}};
    }

    const float bob = std::sin(timeSeconds * 1.4f) * std::min(10.f, side.size.y * 0.045f);
    side.origin.y += bob;

    recordMainCard(context, main, timeSeconds, badge);
    recordSideCard(context, side);

    const float barY = safeArea.origin.y + safeArea.size.y - barH;
    GroupParams glass;
    glass.opacity = 0.42f;
    glass.bounds = Rect{{0.f, 0.f}, {size.x, barH}};
    glass.transform = Mat4::translate(0.f, barY);
    context.pushGroup(glass);
    context.setFillColor(0xe8eef4ff);
    context.fill(Rect::fromSize({size.x, barH}));
    const float chipH = std::clamp(barH - 32.f, 36.f, 48.f);
    const float chipW = std::clamp(safeArea.size.x * 0.38f, 120.f, 200.f);
    const float chipX = safeArea.origin.x + pad;
    const float chipY = (barH - chipH) * 0.5f;
    context.setFillColor(0x2a9d8fff);
    context.fillRounded(Rect{{chipX, chipY}, {chipW, chipH}}, Radius{chipH * 0.35f});
    context.setFillColor(0x243038ff);
    context.strokeRect(Rect{{chipX, chipY}, {chipW, chipH}}, Radius{chipH * 0.35f}, 2.f);
    context.popGroup();
}
