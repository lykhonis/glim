#include <glim/math.h>
#include <glim/paint/Context.h>
#include <glim/paint/Scene.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#ifndef GLIM_GOLDEN
#include "run.h"
#endif

void recordGradients(glim::paint::Context& context, glim::Vec2 size, float timeSeconds,
                     glim::Rect safeArea = {}) {
    using glim::Color;
    using glim::Radius;
    using glim::Rect;
    using glim::Vec2;
    using glim::paint::GradientStop;
    using glim::paint::Matter;

    if (size.x < 1.f || size.y < 1.f) {
        return;
    }
    const Rect win = Rect::fromSize(size);
    const Rect area = safeArea.size.x > 0.f ? safeArea : win;
    const float m = std::min(area.size.x, area.size.y);
    const float pad = std::clamp(m * 0.055f, 14.f, 26.f);

    context.setFillColor(0x14181dff);
    context.fill(win);

    // Linear banner: three stops across the work area.
    const Rect banner{{area.origin.x + pad, area.origin.y + pad}, {area.size.x - pad * 2.f, 120.f}};
    const GradientStop bannerStops[3] = {
        {0.f, Color{0x2a9d8fff}},
        {0.55f, Color{0xe9c46aff}},
        {1.f, Color{0xd97b6bff}},
    };
    context.setFill(
        Matter::linearGradient(banner.origin, {banner.origin.x + banner.size.x, banner.origin.y},
                               bannerStops, 3));
    context.fillRounded(banner, Radius{20.f});

    // Radial disc beside the banner.
    const float discD = 120.f;
    const Vec2 discC{area.origin.x + area.size.x - pad - discD * 0.5f,
                     banner.origin.y + banner.size.y + pad + discD * 0.5f};
    const GradientStop discStops[2] = {
        {0.f, Color{0xf4f1deff}},
        {1.f, Color{0x3d5a80ff}},
    };
    context.setFill(Matter::radialGradient(discC, discD * 0.5f, discStops, 2));
    context.fillRounded(Rect{{discC.x - discD * 0.5f, discC.y - discD * 0.5f}, {discD, discD}},
                        Radius{discD * 0.5f});

    // Gradient ring (stroke) around a solid core.
    const Vec2 ringC{area.origin.x + pad + 70.f, discC.y};
    const GradientStop ringStops[2] = {
        {0.f, Color{0x2a9d8fff}},
        {1.f, Color{0xe9c46aff}},
    };
    context.setFillColor(0x243038ff);
    context.fillRounded(Rect{{ringC.x - 52.f, ringC.y - 52.f}, {104.f, 104.f}}, Radius{26.f});
    context.setFill(Matter::linearGradient({ringC.x - 52.f, ringC.y - 52.f},
                                           {ringC.x + 52.f, ringC.y + 52.f}, ringStops, 2));
    context.strokeRect(Rect{{ringC.x - 44.f, ringC.y - 44.f}, {88.f, 88.f}}, Radius{22.f}, 8.f);

    // Animated sweep: gradient endpoints slide with time.
    const float sweepY = discC.y + discD * 0.5f + pad;
    const Rect sweep{{area.origin.x + pad, sweepY}, {area.size.x - pad * 2.f, 56.f}};
    const float slide = (std::sin(timeSeconds * 0.9f) * 0.5f + 0.5f) * sweep.size.x;
    const GradientStop sweepStops[2] = {
        {0.f, Color{0xe9c46aff}},
        {1.f, Color{0x9b5de5ff}},
    };
    context.setFill(Matter::linearGradient({sweep.origin.x + slide, sweep.origin.y},
                                           {sweep.origin.x + slide + 160.f, sweep.origin.y},
                                           sweepStops, 2));
    context.fillRounded(sweep, Radius{14.f});

    context.setFillColor(0xe8eef4ff);
    context.text({area.origin.x + pad, banner.origin.y + 44.f}, "gradients", 34.f);
}

#ifndef GLIM_GOLDEN
void recordExample(ExampleFrame& frame) {
    recordGradients(frame.context, frame.size, frame.time, frame.safeArea);
}
#endif
