#include <glim/math.h>
#include <glim/paint/Context.h>
#include <glim/paint/Scene.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#ifndef GLIM_GOLDEN
#include "run.h"
#endif

void recordGlass(glim::paint::Context& context, glim::Vec2 size, float timeSeconds,
                 glim::Rect safeArea = {}, bool reduceTransparency = false) {
    using glim::Radius;
    using glim::Rect;
    using glim::paint::Glass;
    using glim::paint::GlassVariant;
    using glim::paint::GroupParams;

    if (size.x < 1.f || size.y < 1.f) {
        return;
    }
    const Rect win = Rect::fromSize(size);
    const Rect area = safeArea.size.x > 0.f ? safeArea : win;
    const float m = std::min(area.size.x, area.size.y);
    const float pad = std::clamp(m * 0.055f, 14.f, 26.f);

    context.setFillColor(0x1a2228ff);
    context.fill(win);

    const float stripe = std::clamp(m * 0.22f, 96.f, 140.f);
    const float period = stripe * 4.f;
    const float scroll = std::fmod(timeSeconds * 13.f, period);
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
        context.setFillColor(c);
        context.fill(Rect{{x, 0.f}, {stripe, win.size.y}});
    }

    const float gap = pad * 0.65f;
    const float pillH = std::clamp(m * 0.15f, 60.f, 88.f);
    const float inner = area.size.x - pad * 2.f;
    const float pillW = std::max(72.f, (inner - gap * 2.f) / 3.f);
    const float pillsY = area.origin.y + 52.f + pad;
    const Rect frostR{{area.origin.x + pad, pillsY}, {pillW, pillH}};
    const Rect clearR{{frostR.origin.x + pillW + gap, pillsY}, {pillW, pillH}};
    const Rect frostedR{{clearR.origin.x + pillW + gap, pillsY}, {pillW, pillH}};

    const float mergeY = pillsY + pillH + pad * 1.35f;
    const float mergeH = std::clamp(m * 0.18f, 72.f, 108.f);
    const float circleS = mergeH;
    const float rectW = std::clamp(circleS * 1.55f, 100.f, 180.f);
    const float mergeT = 0.5f + 0.5f * std::sin(timeSeconds * 0.35f);
    const float mergeGap = -circleS * 0.22f + (pad * 2.4f + circleS * 0.22f) * mergeT;
    const float mergeInner = circleS + mergeGap + rectW;
    const float mergeX = area.origin.x + pad + std::max(0.f, (inner - mergeInner) * 0.5f);
    const Rect circleR{{mergeX, mergeY}, {circleS, circleS}};
    const Rect rectR{{circleR.origin.x + circleS + mergeGap, mergeY + (mergeH - circleS) * 0.5f},
                     {rectW, circleS}};
    const float unionX0 = std::min(circleR.origin.x, rectR.origin.x);
    const float unionY0 = std::min(circleR.origin.y, rectR.origin.y);
    const float unionX1 = std::max(circleR.origin.x + circleR.size.x, rectR.origin.x + rectR.size.x);
    const float unionY1 = std::max(circleR.origin.y + circleR.size.y, rectR.origin.y + rectR.size.y);
    const Rect mergeBox{{unionX0, unionY0}, {unionX1 - unionX0, unionY1 - unionY0}};

    const float orbR = pillH * 0.54f;
    const float orbU = 0.5f + 0.5f * std::sin(timeSeconds * 0.25f);
    const float orbX0 = frostR.origin.x - orbR * 0.2f;
    const float orbX1 = frostedR.origin.x + frostedR.size.x - orbR * 1.8f;
    const float orbX = orbX0 + (orbX1 - orbX0) * orbU;
    const float orbY = pillsY + pillH * 0.87f - orbR;
    context.setFillColor(0xf8fafcff);
    context.fillRounded({{orbX, orbY}, {orbR * 2.f, orbR * 2.f}}, Radius{orbR});

    GroupParams frostP;
    frostP.bounds = frostR;
    frostP.clip = frostR;
    frostP.clipRadius = Radius{pillH * 0.5f};
    frostP.backdropBlur = 28.f;
    frostP.backdropFlat = reduceTransparency;
    context.pushGroup(frostP);
    context.popGroup();

    Glass clear;
    clear.variant = GlassVariant::Clear;
    clear.thicknessPx = 16.f;
    clear.ior = 1.33f;
    clear.dispersion = 0.08f;
    clear.flatten = reduceTransparency;
    GroupParams clearP;
    clearP.bounds = clearR;
    clearP.clip = clearR;
    clearP.clipRadius = Radius{pillH * 0.5f};
    clearP.glass = clear;
    context.pushGroup(clearP);
    context.popGroup();

    Glass frosted;
    frosted.variant = GlassVariant::Regular;
    frosted.flatten = reduceTransparency;
    GroupParams frostedP;
    frostedP.bounds = frostedR;
    frostedP.clip = frostedR;
    frostedP.clipRadius = Radius{pillH * 0.5f};
    frostedP.glass = frosted;
    context.pushGroup(frostedP);
    context.popGroup();

    Glass mergeG;
    mergeG.variant = GlassVariant::Regular;
    mergeG.mergeKPx = 16.f;
    mergeG.flatten = reduceTransparency;
    GroupParams box;
    box.bounds = mergeBox;
    box.glassContainer = true;
    box.glass = mergeG;
    context.pushGroup(box);
    GroupParams circ;
    circ.bounds = circleR;
    circ.clip = circleR;
    circ.clipRadius = Radius{circleS * 0.5f};
    circ.glass = mergeG;
    context.pushGroup(circ);
    context.popGroup();
    GroupParams rnd;
    rnd.bounds = rectR;
    rnd.clip = rectR;
    rnd.clipRadius = Radius{16.f};
    rnd.glass = mergeG;
    context.pushGroup(rnd);
    context.popGroup();
    context.popGroup();

    const float dockH = std::clamp(m * 0.12f, 52.f, 72.f);
    const float dockW = std::min(inner, std::max(280.f, inner * 0.92f));
    const float dockX = area.origin.x + pad + (inner - dockW) * 0.5f;
    const float dockY = area.origin.y + area.size.y - pad - dockH;
    const Rect dockR{{dockX, dockY}, {dockW, dockH}};
    Glass dockG;
    dockG.variant = GlassVariant::Clear;
    dockG.thicknessPx = 20.f;
    dockG.ior = 1.33f;
    dockG.dispersion = 0.08f;
    dockG.flatten = reduceTransparency;
    GroupParams dockP;
    dockP.bounds = dockR;
    dockP.clip = dockR;
    dockP.clipRadius = Radius{dockH * 0.5f};
    dockP.glass = dockG;
    context.pushGroup(dockP);
    context.popGroup();

    const float label = 17.f;
    const std::uint32_t ink = reduceTransparency ? 0xf8fafcff : 0x1a2228ff;
    context.setFillColor(ink);
    const float ly = pillsY + pillH * 0.5f - 7.f;
    context.text({frostR.origin.x + pad * 0.35f, ly}, "frost", label);
    context.text({clearR.origin.x + pad * 0.35f, ly}, "clear", label);
    context.text({frostedR.origin.x + pad * 0.35f, ly}, "frosted", label);
    context.text({circleR.origin.x + 8.f, circleR.origin.y + circleS * 0.5f - 8.f}, "merge", label);
}

#ifndef GLIM_GOLDEN
void recordExample(ExampleFrame& frame) {
    recordGlass(frame.context, frame.size, frame.time, frame.safeArea, frame.reduceTransparency);
}
#endif
