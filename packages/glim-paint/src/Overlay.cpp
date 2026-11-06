#include <glim/paint/Context.h>
#include <glim/paint/Overlay.h>

#include <algorithm>
#include <cstdio>

namespace glim::paint {

void Overlay::setAvoid(const Rect* rects, int count) {
  avoidCount_ = 0;
  if (!rects || count <= 0) {
    return;
  }
  if (count > kMaxAvoid) {
    count = kMaxAvoid;
  }
  for (int i = 0; i < count; ++i) {
    avoid_[i] = rects[i];
  }
  avoidCount_ = count;
}

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
  // 2 Hz snapshot throttle keeps records bit-identical; display lags one snapshot.
  statsAge_ += dtSeconds;
  while (statsAge_ >= kStatsInterval) {
    statsDisplayed_ = statsPending_;
    recordDisplayed_ = recordPending_;
    statsAge_ -= kStatsInterval;
    statsChanged_ = true;
  }
}

void Overlay::drawText(
    Context& context, Vec2 topLeft, const char* text, float sizePx, Color color
) {
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

  constexpr float kPad = 10.f;
  constexpr float kRowPx = 13.f;
  constexpr float kRowH = 18.f;
  constexpr float kSparkH = 26.f;
  constexpr Color kWhite{0xf2f5f7ff};
  const float y0 = safeArea.size.y > 0.f ? safeArea.origin.y : 0.f;

  char line[96];
  const int fpsI = std::max(0, static_cast<int>(fps_ + 0.5f));
  if (!expanded_) {
    std::snprintf(line, sizeof(line), "FPS %d  %.1f ms", fpsI, static_cast<double>(lastFrameMs_));
    const TextSize m = context.measureText(line, kRowPx);
    const float chipW = (m.width > 0.f ? m.width : 120.f) + kPad * 2.f;
    const float chipH = kRowPx + kPad;
    // Walk TL→TR→BL→BR for the first corner with no seat under it.
    const float height = size.y > 0.f ? size.y : safeArea.size.y;
    const Vec2 spots[4] = {
        {kPad, y0 + kPad},
        {width - kPad - chipW, y0 + kPad},
        {kPad, y0 + height - kPad - chipH},
        {width - kPad - chipW, y0 + height - kPad - chipH},
    };
    Vec2 at = spots[0];
    for (const Vec2& s : spots) {
      const Rect chip{s, {chipW, chipH}};
      bool hit = false;
      for (int i = 0; i < avoidCount_; ++i) {
        const Rect& a = avoid_[i];
        const float mgn = 4.f;
        if (s.x < a.origin.x + a.size.x + mgn && s.x + chipW + mgn > a.origin.x &&
            s.y < a.origin.y + a.size.y + mgn && s.y + chipH + mgn > a.origin.y) {
          hit = true;
          break;
        }
      }
      if (!hit) {
        at = s;
        break;
      }
    }
    context.setFillColor(0x0d1117b3);
    context.fillRounded({at, {chipW, chipH}}, Radius{8.f});
    drawText(context, {at.x + kPad, at.y + (chipH - kRowPx) * 0.5f}, line, kRowPx, kWhite);
    statsChanged_ = false;
    return;
  }
  const int rows = 14;
  const float bannerH = kPad + rows * kRowH + 6.f + kSparkH + kPad;
  context.setFillColor(0x0d1117d9);
  context.fill({{0.f, y0}, {width, bannerH}});

  float y = y0 + kPad;
  const auto row = [&](const char* fmt, auto v) {
    std::snprintf(line, sizeof(line), fmt, v);
    drawText(context, {kPad, y}, line, kRowPx, kWhite);
    y += kRowH;
  };
  const auto row2 = [&](const char* fmt, auto a, auto b) {
    std::snprintf(line, sizeof(line), fmt, a, b);
    drawText(context, {kPad, y}, line, kRowPx, kWhite);
    y += kRowH;
  };
  row("FPS %d", fpsI);
  row("frame %.1f ms", static_cast<double>(lastFrameMs_));
  row("record %.1f ms", static_cast<double>(recordDisplayed_));
  row("encode %.1f ms", static_cast<double>(statsDisplayed_.encodeMs));
  row2("size %.0f x %.0f", static_cast<double>(size.x), static_cast<double>(size.y));
  row("draws %u", statsDisplayed_.draws);
  row("instances %u", statsDisplayed_.instances);
  row("isolates %u", statsDisplayed_.isolateCount);
  row("merged %u", statsDisplayed_.mergedGroupCount);
  row2("tiles %u (dirty %u)", statsDisplayed_.tileCount, statsDisplayed_.dirtyTiles);
  row("backdrop %u", statsDisplayed_.backdropCount);
  row2("glass %u x %u pills", statsDisplayed_.glassPassCount, statsDisplayed_.glassPillCount);
  row2("reuse %u / %u miss", statsDisplayed_.reuseHits, statsDisplayed_.reuseMisses);
  std::snprintf(
      line,
      sizeof(line),
      "safe %.0f %.0f %.0f x %.0f",
      static_cast<double>(safeArea.origin.x),
      static_cast<double>(safeArea.origin.y),
      static_cast<double>(safeArea.size.x),
      static_cast<double>(safeArea.size.y)
  );
  drawText(context, {kPad, y}, line, kRowPx, kWhite);
  y += kRowH;

  const float sparkW = width - kPad * 2.f;
  const float bar = sparkW / static_cast<float>(kSpark);
  if (sparkCount_ > 0 && bar > 0.f) {
    const int n = sparkCount_;
    const int start = sparkCount_ < kSpark ? 0 : sparkHead_;
    for (int i = 0; i < n; ++i) {
      const float ms = spark_[(start + i) % kSpark];
      const float h = std::min(kSparkH, std::max(1.f, ms * (kSparkH / 22.f)));
      context.setFillColor(ms > 16.7f * 1.5f ? Color{0xe76f51ff} : Color{0x2a9d8fff});
      context.fill({{kPad + static_cast<float>(i) * bar, y + kSparkH - h}, {bar, h}});
    }
  }
  statsChanged_ = false;
}

}  // namespace glim::paint
