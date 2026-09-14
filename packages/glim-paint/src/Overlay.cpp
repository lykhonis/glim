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

    const Vec2 size = context.size();
    const float width = size.x > 0.f ? size.x : safeArea.size.x;
    if (width <= 0.f) {
        return;
    }

    constexpr float kH = 44.f;
    constexpr float kPad = 10.f;
    constexpr float kGap = 14.f;
    constexpr float kFpsPx = 16.f;
    constexpr float kMetaPx = 13.f;
    const float y0 = safeArea.size.y > 0.f ? safeArea.origin.y : 0.f;
    const Rect banner{{0.f, y0}, {width, kH}};
    context.setFillColor(0x0d111766);
    context.fill(banner);

    char line[48];
    const int fpsI = std::max(0, static_cast<int>(fps_ + 0.5f));
    std::snprintf(line, sizeof(line), "FPS %d", fpsI);
    const TextSize fpsM = context.measureText(line, kFpsPx);
    float x = kPad;
    const float textY = y0 + (kH - (fpsM.height > 0.f ? fpsM.height : kFpsPx)) * 0.5f;
    drawText(context, {x, textY}, line, kFpsPx, Color{0xe8eef2ff});
    x += (fpsM.width > 0.f ? fpsM.width : 72.f) + kGap;

    std::snprintf(line, sizeof(line), "%.1f ms", static_cast<double>(lastFrameMs_));
    const TextSize msM = context.measureText(line, kMetaPx);
    drawText(context, {x, textY + 2.f}, line, kMetaPx, Color{0xc5ccd1ff});
    x += (msM.width > 0.f ? msM.width : 48.f) + kGap;

    std::snprintf(line, sizeof(line), "%.1f %ud %ui %uo", static_cast<double>(stats_.encodeMs),
                  stats_.draws, stats_.instances, stats_.isolateCount);
    const TextSize stM = context.measureText(line, kMetaPx);
    drawText(context, {x, textY + 2.f}, line, kMetaPx, Color{0xa8b3b8ff});
    x += (stM.width > 0.f ? stM.width : 88.f) + kGap;

    constexpr float kSparkMax = 132.f;
    const float sparkW = std::min(kSparkMax, std::max(8.f, width - kPad - x));
    const float sx = width - kPad - sparkW;
    const float sparkH = kH - 6.f;
    const float sy = y0 + 3.f;
    const float bar = sparkW / static_cast<float>(kSpark);
    if (sparkCount_ > 0 && bar > 0.f) {
        const int n = sparkCount_;
        const int start = sparkCount_ < kSpark ? 0 : sparkHead_;
        for (int i = 0; i < n; ++i) {
            const float ms = spark_[(start + i) % kSpark];
            const float h = std::min(sparkH, std::max(1.f, ms * (sparkH / 22.f)));
            const bool slow = ms > 16.7f * 1.5f;
            context.setFillColor(slow ? 0xe76f51ff : 0x2a9d8fff);
            context.fill(Rect{{sx + static_cast<float>(i) * bar, sy + sparkH - h}, {bar, h}});
        }
    }
}

}  // namespace glim::paint
