#include <glim/math.h>
#include <glim/paint/Context.h>

#include <cmath>
#include <cstdint>

#ifndef GLIM_WEB_NO_RUNNER
#include "run_web.h"
#endif

namespace {

void recordCard(glim::paint::Context& context, glim::Vec2 size, float time) {
    using glim::Radius;
    using glim::Rect;

    context.setFillColor(0x243038ff);
    context.fill(Rect::fromSize(size));

    const float w = size.x * 0.62f;
    const float h = size.y * 0.52f;
    const Rect card{{(size.x - w) * 0.5f, (size.y - h) * 0.5f}, {w, h}};
    context.setFillColor(0xac6363ff);
    context.fillRounded(card, Radius{24.f});

    const float bob = std::sin(time * 1.4f) * 8.f;
    const Rect chip{{card.origin.x + 24.f, card.origin.y + 28.f + bob}, {160.f, 44.f}};
    context.setFillColor(0x2a9d8fff);
    context.fillRounded(chip, Radius{16.f});

    context.setFillColor(0xf0d5a8ff);
    context.text({card.origin.x + 24.f, card.origin.y + 120.f}, "hello web", 28.f);
}

}  // namespace

void recordWebExample(glim::paint::Context& context, glim::Vec2 size, float time) {
    if (size.x < 1.f || size.y < 1.f) {
        return;
    }
    recordCard(context, size, time);
}

#ifndef GLIM_WEB_NO_RUNNER
void recordExample(ExampleFrame& frame) {
    recordWebExample(frame.context, frame.size, frame.time);
}
#endif
