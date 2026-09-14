#include "run.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

void recordExample(ExampleFrame& frame) {
    using glim::Radius;
    using glim::Rect;
    using glim::paint::GroupParams;

    const Rect win = Rect::fromSize(frame.size);
    const Rect area = frame.safeArea.size.x > 0.f ? frame.safeArea : win;
    const float m = std::min(area.size.x, area.size.y);
    const float pad = std::clamp(m * 0.06f, 14.f, 28.f);

    frame.context.setFillColor(0x1a2228ff);
    frame.context.fill(win);

    const float stripe = 44.f;
    const float period = stripe * 4.f;
    const float scroll = std::fmod(frame.time * 28.f, period);
    const float origin = -scroll;
    const int first = static_cast<int>(std::floor(-origin / stripe)) - 1;
    const int last = static_cast<int>(std::ceil((win.size.x - origin) / stripe)) + 1;
    for (int i = first; i <= last; ++i) {
        const float x = origin + static_cast<float>(i) * stripe;
        const int k = ((i % 4) + 4) % 4;
        const std::uint32_t c = k == 0   ? 0xd97b6bff
                                : k == 1 ? 0x2a9d8fff
                                : k == 2 ? 0xe9c46aff
                                         : 0x3d5a80ff;
        frame.context.setFillColor(c);
        frame.context.fill(Rect{{x, 0.f}, {stripe, win.size.y}});
    }

    const float gap = pad * 0.7f;
    const float barH = std::clamp(m * 0.11f, 48.f, 64.f);
    const float pillH = std::clamp(m * 0.16f, 64.f, 96.f);
    const float inner = area.size.x - pad * 2.f;
    const float pillW = std::max(64.f, (inner - gap * 2.f) / 3.f);
    const float pillsY = area.origin.y + 52.f + pad;
    const Rect clearR{{area.origin.x + pad, pillsY}, {pillW, pillH}};
    const Rect glassR{{clearR.origin.x + pillW + gap, pillsY}, {pillW, pillH}};
    const Rect frostR{{glassR.origin.x + pillW + gap, pillsY}, {pillW, pillH}};
    const Rect barR{{area.origin.x + pad, area.origin.y + area.size.y - pad - barH},
                    {area.size.x - pad * 2.f, barH}};

    const float driftX = std::sin(frame.time * 0.65f) * std::min(64.f, area.size.x * 0.12f);
    const float driftY = std::cos(frame.time * 0.48f) * std::min(36.f, area.size.y * 0.06f);
    frame.context.setFillColor(0xf4f1deff);
    frame.context.fillRounded({{area.origin.x + pad + driftX,
                               pillsY + pillH + pad * 0.8f + driftY},
                               {area.size.x * 0.34f, pad * 2.2f}},
                              Radius{12.f});
    frame.context.setFillColor(0x0d1b2aff);
    frame.context.fillRounded(
        {{area.origin.x + area.size.x * 0.42f - driftX * 0.6f,
          barR.origin.y - pad * 2.8f + driftY * 0.8f},
         {area.size.x * 0.4f, pad * 3.2f}},
        Radius{16.f});

    GroupParams clearP;
    clearP.bounds = clearR;
    clearP.clip = clearR;
    clearP.clipRadius = Radius{pillH * 0.5f};
    clearP.backdropBlur = 4.f;
    clearP.backdropBend = 0.85f;
    clearP.backdropFlat = frame.reduceTransparency;
    frame.context.pushGroup(clearP);
    frame.context.popGroup();

    GroupParams glassP;
    glassP.bounds = glassR;
    glassP.clip = glassR;
    glassP.clipRadius = Radius{pillH * 0.5f};
    glassP.backdropBlur = 28.f;
    glassP.backdropBend = 0.7f;
    glassP.backdropFlat = frame.reduceTransparency;
    frame.context.pushGroup(glassP);
    frame.context.popGroup();

    GroupParams frostP;
    frostP.bounds = frostR;
    frostP.clip = frostR;
    frostP.clipRadius = Radius{pillH * 0.5f};
    frostP.backdropBlur = 28.f;
    frostP.backdropFlat = frame.reduceTransparency;
    frame.context.pushGroup(frostP);
    frame.context.popGroup();

    GroupParams bar;
    bar.bounds = barR;
    bar.clip = barR;
    bar.clipRadius = Radius{barH * 0.5f};
    bar.backdropBlur = 28.f;
    bar.backdropFlat = frame.reduceTransparency;
    frame.context.pushGroup(bar);
    frame.context.popGroup();

    const float label = 18.f;
    const std::uint32_t ink = frame.reduceTransparency ? 0xf8fafcff : 0x1a2228ff;
    frame.context.setFillColor(ink);
    frame.context.text({clearR.origin.x + pad * 0.35f, clearR.origin.y + pillH * 0.5f - 7.f}, "clear",
                       label);
    frame.context.text({glassR.origin.x + pad * 0.35f, glassR.origin.y + pillH * 0.5f - 7.f}, "frosted",
                       label);
    frame.context.text({frostR.origin.x + pad * 0.35f, frostR.origin.y + pillH * 0.5f - 7.f}, "frost",
                       label);
    frame.context.text({barR.origin.x + pad * 0.6f, barR.origin.y + barH * 0.5f - 8.f},
                       frame.reduceTransparency ? "flat" : "backdrop", label);
}
