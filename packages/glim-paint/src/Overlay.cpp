#include <glim/paint/Overlay.h>

#include <glim/paint/Context.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace glim::paint {
namespace {

constexpr int kGlyphW = 5;
constexpr int kGlyphH = 7;
constexpr int kCellW = 6;
constexpr int kCellH = 8;
constexpr char kCharset[] = "0123456789.FPSmsdio ";
constexpr int kGlyphCount = static_cast<int>(sizeof(kCharset) - 1);

// 5x7, bit 4 is the left pixel.
constexpr std::uint8_t kFont[kGlyphCount][kGlyphH] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},  // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},  // 1
    {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F},  // 2
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},  // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},  // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},  // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},  // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},  // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},  // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},  // 9
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06},  // .
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},  // F
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},  // P
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},  // S
    {0x00, 0x00, 0x1A, 0x15, 0x15, 0x15, 0x15},  // m
    {0x00, 0x00, 0x0E, 0x10, 0x0E, 0x01, 0x1E},  // s
    {0x01, 0x01, 0x0F, 0x11, 0x11, 0x11, 0x0F},  // d
    {0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E},  // i
    {0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E},  // o
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // space
};

int glyphIndex(char c) {
    const char* p = std::strchr(kCharset, c);
    if (!p) {
        return -1;
    }
    return static_cast<int>(p - kCharset);
}

std::vector<std::uint8_t> makeAtlas() {
    const int w = kGlyphCount * kCellW;
    const int h = kCellH;
    std::vector<std::uint8_t> px(static_cast<std::size_t>(w * h * 4), 0);
    for (int g = 0; g < kGlyphCount; ++g) {
        for (int row = 0; row < kGlyphH; ++row) {
            const std::uint8_t bits = kFont[g][row];
            for (int col = 0; col < kGlyphW; ++col) {
                if ((bits & (1 << (kGlyphW - 1 - col))) == 0) {
                    continue;
                }
                const int x = g * kCellW + col;
                const int y = row;
                const std::size_t i = static_cast<std::size_t>((y * w + x) * 4);
                px[i + 0] = 255;
                px[i + 1] = 255;
                px[i + 2] = 255;
                px[i + 3] = 255;
            }
        }
    }
    return px;
}

}  // namespace

void Overlay::tick(float dtSeconds) {
    if (!started_) {
        started_ = true;
        return;
    }
    if (dtSeconds <= 0.f || dtSeconds > 1.f) {
        return;
    }
    lastFrameMs_ = dtSeconds * 1000.f;
    spark_[sparkHead_] = lastFrameMs_;
    sparkHead_ = (sparkHead_ + 1) % kSpark;
    if (sparkCount_ < kSpark) {
        sparkCount_ += 1;
    }
    windowTime_ += dtSeconds;
    windowFrames_ += 1;
    if (windowTime_ >= 1.f) {
        fps_ = static_cast<float>(windowFrames_) / windowTime_;
        windowTime_ = 0.f;
        windowFrames_ = 0;
    } else if (fps_ <= 0.f) {
        fps_ = 1.f / dtSeconds;
    }
}

void Overlay::ensureAtlas(Context& context) {
    if (atlasId_ != 0) {
        return;
    }
    const std::vector<std::uint8_t> px = makeAtlas();
    atlasId_ = context.addImage(kGlyphCount * kCellW, kCellH, px.data());
}

void Overlay::drawText(Context& context, Vec2 origin, const char* text, float scale, Color color) {
    if (!text || atlasId_ == 0) {
        return;
    }
    const float atlasW = static_cast<float>(kGlyphCount * kCellW);
    const float atlasH = static_cast<float>(kCellH);
    float x = origin.x;
    for (const char* p = text; *p; ++p) {
        const int gi = glyphIndex(*p);
        if (gi < 0) {
            x += static_cast<float>(kCellW) * scale;
            continue;
        }
        Matter m = Matter::sampled(atlasId_, color);
        m.uv = {{static_cast<float>(gi * kCellW) / atlasW, 0.f},
                {static_cast<float>(kGlyphW) / atlasW, static_cast<float>(kGlyphH) / atlasH}};
        context.blit(Rect{{x, origin.y}, {static_cast<float>(kGlyphW) * scale, static_cast<float>(kGlyphH) * scale}},
                     m);
        x += static_cast<float>(kCellW) * scale;
    }
}

void Overlay::record(Context& context, Rect safeArea) {
    if (!enabled_) {
        return;
    }
    ensureAtlas(context);

    constexpr float kPad = 10.f;
    constexpr float kGap = 4.f;
    constexpr float kSparkH = 16.f;
    constexpr float kBar = 2.f;
    constexpr float kSparkW = static_cast<float>(kSpark) * kBar;
    const float scale = 2.f;
    const float line1 = static_cast<float>(kGlyphH) * scale;
    const float line2 = static_cast<float>(kGlyphH);
    const float width = kPad * 2.f + kSparkW;
    const float height = kPad * 2.f + line1 + kGap + line2 + kGap + kSparkH + kGap + line2;
    const float x0 = safeArea.origin.x + 8.f;
    const float y0 = safeArea.origin.y + 8.f;
    if (safeArea.size.x <= 0.f || safeArea.size.y <= 0.f) {
        return;
    }

    const Rect panel{{x0, y0}, {width, height}};
    context.setFillColor(0x0d1117ff);
    context.fillRounded(panel, Radius{8.f});
    context.setFillColor(0x2a9d8fff);
    context.strokeRect(panel, Radius{8.f}, 1.5f);

    char line[32];
    const int fpsI = std::max(0, static_cast<int>(fps_ + 0.5f));
    std::snprintf(line, sizeof(line), "FPS %d", fpsI);
    drawText(context, {x0 + kPad, y0 + kPad}, line, scale, Color{0xe8eef2ff});

    std::snprintf(line, sizeof(line), "%.1f ms", static_cast<double>(lastFrameMs_));
    drawText(context, {x0 + kPad, y0 + kPad + line1 + kGap}, line, 1.f, Color{0xc5ccd1ff});

    const float sx = x0 + kPad;
    const float sy = y0 + kPad + line1 + kGap + line2 + kGap;
    context.setFillColor(0x0b0e11ff);
    context.fill(Rect{{sx, sy}, {kSparkW, kSparkH}});
    if (sparkCount_ > 0) {
        const int n = sparkCount_;
        const int start = sparkCount_ < kSpark ? 0 : sparkHead_;
        for (int i = 0; i < n; ++i) {
            const float ms = spark_[(start + i) % kSpark];
            const float h = std::min(kSparkH, std::max(1.f, ms * (kSparkH / 33.4f)));
            const bool slow = ms > 16.7f * 1.5f;
            context.setFillColor(slow ? 0xe76f51ff : 0x2a9d8fff);
            context.fill(Rect{{sx + static_cast<float>(i) * kBar, sy + kSparkH - h}, {kBar, h}});
        }
    }

    std::snprintf(line, sizeof(line), "%.1f %ud %ui %uo", static_cast<double>(stats_.encodeMs),
                  stats_.draws, stats_.instances, stats_.isolateCount);
    drawText(context, {x0 + kPad, sy + kSparkH + kGap}, line, 1.f, Color{0xa8b3b8ff});
}

}  // namespace glim::paint
