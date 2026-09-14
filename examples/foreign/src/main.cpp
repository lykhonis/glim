#include "run.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

glim::gpu::Texture gTex;
std::uint32_t gId = 0;

void ensureTexture(ExampleFrame& frame) {
    if (gId != 0) {
        return;
    }
    constexpr int k = 64;
    auto created = frame.device.createTexture({k, k});
    if (!created.ok()) {
        return;
    }
    gTex = std::move(created.value());
    std::vector<std::uint8_t> px(static_cast<std::size_t>(k * k * 4));
    for (int y = 0; y < k; ++y) {
        for (int x = 0; x < k; ++x) {
            const bool cell = ((x / 8) + (y / 8)) % 2 == 0;
            const std::size_t i = static_cast<std::size_t>((y * k + x) * 4);
            px[i + 0] = cell ? 0x2a : 0xf0;
            px[i + 1] = cell ? 0x9d : 0xd5;
            px[i + 2] = cell ? 0x8f : 0xa8;
            px[i + 3] = 0xff;
        }
    }
    frame.device.writeTexture(gTex, px.data(), px.size());
    gId = frame.context.wrapNativeTexture(gTex.native(), k, k, glim::paint::SampleFormat::Rgba8Unorm);
}

}  // namespace

void recordExample(ExampleFrame& frame) {
    using glim::Radius;
    using glim::Rect;

    frame.context.setFillColor(0x334c4cff);
    frame.context.fill(Rect::fromSize(frame.size));

    ensureTexture(frame);

    const Rect area = frame.safeArea.size.x > 0.f ? frame.safeArea : Rect::fromSize(frame.size);
    const float m = std::min(area.size.x, area.size.y);
    const float pad = std::clamp(m * 0.08f, 16.f, 32.f);
    const Rect card{{area.origin.x + pad, area.origin.y + pad},
                    {std::max(8.f, area.size.x - pad * 2.f), std::max(8.f, area.size.y - pad * 2.f)}};
    frame.context.setFillColor(0x1d2a2aff);
    frame.context.fillRounded(card, Radius{16.f});

    const float label = 28.f;
    const float inner = pad * 0.55f;
    if (gId != 0) {
        frame.context.blit(Rect{{card.origin.x + inner, card.origin.y + inner},
                                {std::max(8.f, card.size.x - inner * 2.f),
                                 std::max(8.f, card.size.y - inner * 2.f - label)}},
                           glim::paint::Matter::foreign(gId));
    }

    frame.context.setFillColor(0xf0d5a8ff);
    frame.context.strokeRect(card, Radius{16.f}, 3.f);
    frame.context.text({card.origin.x + inner, card.origin.y + card.size.y - 22.f}, "foreign Matter",
                       18.f);
}
