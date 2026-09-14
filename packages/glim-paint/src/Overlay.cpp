#include <glim/paint/Overlay.h>

#include <glim/paint/Context.h>

#include <algorithm>
#include <cstdio>

namespace glim::paint {

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

void Overlay::drawText(Context& context, Vec2 topLeft, const char* text, float sizePx, Color color) {
    if (!text || sizePx <= 0.f) {
        return;
    }
    const TextSize m = context.measureText(text, sizePx);
    context.setFill(Matter::solid(color));
    context.text({topLeft.x, topLeft.y + m.ascent}, text, sizePx);
}

void Overlay::record(Context& context, Rect safeArea) {
    if (!enabled_) {
        return;
    }

    constexpr float kPad = 10.f;
    constexpr float kGap = 4.f;
    constexpr float kSparkH = 16.f;
    constexpr float kBar = 2.f;
    constexpr float kSparkW = static_cast<float>(kSpark) * kBar;
    constexpr float kFpsPx = 18.f;
    constexpr float kLinePx = 12.f;
    const TextSize fpsM = context.measureText("FPS 000", kFpsPx);
    const TextSize lineM = context.measureText("00.0 ms", kLinePx);
    const float line1 = fpsM.height > 0.f ? fpsM.height : kFpsPx;
    const float line2 = lineM.height > 0.f ? lineM.height : kLinePx;
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
    drawText(context, {x0 + kPad, y0 + kPad}, line, kFpsPx, Color{0xe8eef2ff});

    std::snprintf(line, sizeof(line), "%.1f ms", static_cast<double>(lastFrameMs_));
    drawText(context, {x0 + kPad, y0 + kPad + line1 + kGap}, line, kLinePx, Color{0xc5ccd1ff});

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
    drawText(context, {x0 + kPad, sy + kSparkH + kGap}, line, kLinePx, Color{0xa8b3b8ff});
}

}  // namespace glim::paint
