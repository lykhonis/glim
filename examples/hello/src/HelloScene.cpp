#include "HelloScene.h"

#include <algorithm>
#include <cmath>

void recordHello(glim::paint::Context& context, glim::Vec2 size, float timeSeconds) {
    using glim::Mat4;
    using glim::Rect;
    using glim::paint::GroupParams;

    context.setSize(size);
    context.beginFrame();

    context.setFillColor(0x243038ff);
    context.fill(Rect::fromSize(size));

    context.save();
    context.translate({100.f, 50.f});
    context.setFillColor(0xac6363ff);
    context.fill(Rect::fromSize({400.f, 300.f}));
    context.restore();

    const float bob = std::sin(timeSeconds * 1.4f) * 10.f;
    const float sideX = std::max(520.f, size.x - 220.f);
    context.save();
    context.translate({sideX, 70.f + bob});
    context.setFillColor(0x3d7ea6ff);
    context.fill(Rect::fromSize({180.f, 200.f}));
    context.restore();

    context.save();
    context.translate({140.f, 80.f});
    context.setFillColor(0xf0d5a8ff);
    context.fill(Rect::fromSize({80.f, 48.f}));
    context.restore();

    GroupParams glass;
    glass.opacity = 0.42f;
    glass.bounds = Rect{{0, 0}, {size.x, 96.f}};
    glass.transform = Mat4::translate(0, size.y - 96.f);
    context.pushGroup(glass);
    context.setFillColor(0xe8eef4ff);
    context.fill(Rect::fromSize({size.x, 96.f}));
    context.setFillColor(0x2a9d8fff);
    context.fill(Rect{{24.f, 28.f}, {160.f, 40.f}});
    context.popGroup();

    context.finish();
}
