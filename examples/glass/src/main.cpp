#include "run.h"

#include <algorithm>

void recordExample(ExampleFrame& frame) {
    using glim::Radius;
    using glim::Rect;
    using glim::paint::GroupParams;

    frame.context.setFillColor(0x243038ff);
    frame.context.fill(Rect::fromSize(frame.size));

    const Rect area = frame.safeArea.size.x > 0.f ? frame.safeArea : Rect::fromSize(frame.size);
    const float m = std::min(area.size.x, area.size.y);
    const float pad = std::clamp(m * 0.06f, 14.f, 28.f);
    const float stripe = std::max(10.f, m * 0.045f);
    for (int i = 0; i < 24; ++i) {
        const float x = area.origin.x + static_cast<float>(i) * stripe;
        frame.context.setFillColor((i % 2) == 0 ? 0xac6363ff : 0x2a9d8fff);
        frame.context.fill(Rect{{x, area.origin.y}, {stripe, area.size.y}});
    }

    const float gap = pad * 0.7f;
    const float cardW = std::max(8.f, (area.size.x - pad * 2.f - gap) * 0.5f);
    const float cardH = std::max(8.f, area.size.y - pad * 2.f);
    const Rect frostR{{area.origin.x + pad, area.origin.y + pad}, {cardW, cardH}};
    const Rect lensR{{frostR.origin.x + cardW + gap, frostR.origin.y}, {cardW, cardH}};

    GroupParams frost;
    frost.bounds = frostR;
    frost.backdropBlur = 8.f;
    frame.context.pushGroup(frost);
    frame.context.setFillColor(0xf0d5a866);
    frame.context.fillRounded(frostR, Radius{20.f});
    frame.context.popGroup();

    GroupParams lens;
    lens.bounds = lensR;
    lens.backdropBlur = 8.f;
    lens.backdropBend = 0.45f;
    frame.context.pushGroup(lens);
    frame.context.setFillColor(0xe8eef466);
    frame.context.fillRounded(lensR, Radius{20.f});
    frame.context.popGroup();

    frame.context.setFillColor(0xf0d5a8ff);
    frame.context.strokeRect(frostR, Radius{20.f}, 2.f);
    frame.context.strokeRect(lensR, Radius{20.f}, 2.f);
    frame.context.text({frostR.origin.x + pad * 0.45f, frostR.origin.y + frostR.size.y - 24.f}, "blur",
                       18.f);
    frame.context.text({lensR.origin.x + pad * 0.45f, lensR.origin.y + lensR.size.y - 24.f}, "lookalike",
                       18.f);
}
