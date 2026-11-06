#pragma once

// StripChart: scrolling frame-time strip with a 1px budget line at
// 16.7ms and hitch ticks above the warn threshold. Null/empty hub
// renders a static placeholder series so headless renders stay
// deterministic. Read-only, never focusable.
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ShowcaseTheme.h"
#include "glim/toolkit.h"
#include "glim/toolkit/host/Telemetry.h"

namespace glim {
namespace toolkit {

struct StripChart {
  static constexpr const char* kKind = "Strip";
  std::vector<float> ms;  // frame times, oldest-first
  float budgetMs = 16.7f;
  float warnMs = 20.0f;
  float height = 96.0f;
  std::size_t maxSamples = 60;
  Box place(Constraints c, const Engine&) const {
    float w = 240.0f;
    if (!isUnbounded(c.maxW)) w = c.maxW;
    return detail::resolveSize(w, height, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 4);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    // HUD trace: thin monochrome bars on the bare card face, red
    // ticks at/above the warn threshold. A 30px left gutter holds
    // the ms scale so labels never sit under bars.
    const float lo = 4.0f, hi = box.h - 10.0f;
    const float span = 40.0f;  // 0..40ms maps the plot height
    const std::uint32_t ink =
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight);
    auto yOf = [&](float v) {
      float t = v / span;
      if (t < 0.f) t = 0.f;
      if (t > 1.f) t = 1.f;
      return box.y + hi - t * (hi - lo);
    };
    // Plot origin past the scale gutter; budget line spans the plot.
    const float px = box.x + 30.0f;
    const float pw = box.w - 30.0f - 12.0f;
    Box budget{px, yOf(budgetMs), pw, 1.0f};
    sink.fillRounded(
        budget, 1, themeColor(e, kShowCardStroke, Show::kCardStroke)
    );
    // HUD scale: left-edge ms annotations (cleared below the
    // floating glass title strip) + right-edge budget tag.
    {
      const std::uint32_t dim = withAlpha(ink, 0.45f);
      const float size =
          themeFloat(e, kThemeTypeLine, Tokens::kTypeLine);
      sink.text(box.x + 6.0f, box.y + 30.0f, "40", dim, size);
      sink.text(box.x + 6.0f, box.y + hi - size, "0", dim, size);
      sink.text(
          box.x + box.w - 52.0f,
          yOf(budgetMs) - size - 2.0f,
          "16.7",
          dim,
          size
      );
    }
    const std::size_t n =
        ms.empty() ? 0 : std::min(ms.size(), maxSamples);
    const std::size_t skip = ms.empty() ? 0 : ms.size() - n;
    if (n == 0) return;
    const float slot = pw / static_cast<float>(maxSamples);
    const float bw = slot > 3.0f ? slot - 2.0f : slot;
    for (std::size_t i = 0; i < n; ++i) {
      float v = ms[skip + i];
      const float y = yOf(v);
      Box bar{
          px + static_cast<float>(i) * slot + 1.0f,
          y,
          bw,
          box.y + hi - y
      };
      const bool hot = v >= warnMs;
      // Monochrome data ink: ink at 40% reads on both ramps.
      sink.fillRounded(
          bar,
          2,
          hot ? themeColor(e, kShowBad, Show::kBad)
              : withAlpha(ink, 0.4f)
      );
    }
  }
};

inline std::vector<float> stripMs(const host::TelemetryHub* hub) {
  if (hub == nullptr || hub->empty()) {
    return {
        8.0f,
        11.0f,
        9.0f,
        14.0f,
        12.0f,
        17.0f,
        13.0f,
        10.0f,
        15.0f,
        12.0f,
        9.0f,
        14.0f,
        18.0f,
        11.0f,
        10.0f,
        13.0f
    };
  }
  std::vector<float> out;
  auto hist = hub->history();
  out.reserve(hist.size());
  for (const auto& s : hist) out.push_back(s.frameMs);
  return out;
}

template <class... As>
auto stripChart(StripChart s, As&&... as) {
  configureTags(s, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
