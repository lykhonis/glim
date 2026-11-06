#pragma once

// StageRow: pipeline health — Shell → Toolkit → Paint → GPU. Status
// dot (threshold-colored circle), label, live ms. Read-only.
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "ShowcaseTheme.h"
#include "glim/toolkit.h"
#include "glim/toolkit/host/Telemetry.h"

namespace glim {
namespace toolkit {

struct StageRow {
  static constexpr const char* kKind = "StageRow";
  struct Stage {
    std::string label;
    float ms = 0.f;
    float warn = 20.0f;
    float bad = 33.0f;
  };
  std::vector<Stage> stages;
  Box place(Constraints c, const Engine& e) const {
    float w = 0;
    for (const auto& s : stages) {
      Box t = text::measureText(
          s.label + " 00.0", {0, kUnbounded, 0, kUnbounded}, e
      );
      w += 16.0f + t.w + 28.0f;
    }
    if (!isUnbounded(c.maxW)) w = c.maxW;
    return detail::resolveSize(w, 28.0f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 8);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const std::size_t n = stages.size();
    if (n == 0) return;
    const float slot = box.w / static_cast<float>(n);
    const float cy = box.y + box.h * 0.5f;
    const std::uint32_t ink =
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight);
    const std::uint32_t faint =
        themeColor(e, kShowCardStroke, Show::kCardStroke);
    const float size = themeFloat(e, kThemeTypeLine, Tokens::kTypeLine);
    for (std::size_t i = 0; i < n; ++i) {
      const Stage& s = stages[i];
      const float x = box.x + static_cast<float>(i) * slot + 12.0f;
      // Connector hairline to the next stage (not after the last).
      if (i + 1 < n) {
        const float x1 =
            box.x + static_cast<float>(i + 1) * slot + 12.0f;
        sink.fillRounded(
            {x + 8.0f, cy - 0.5f, x1 - x - 8.0f - 8.0f, 1.0f}, 1, faint
        );
      }
      std::uint32_t dot = themeColor(e, kShowOk, Show::kOk);
      if (s.ms >= s.bad)
        dot = themeColor(e, kShowBad, Show::kBad);
      else if (s.ms >= s.warn)
        dot = themeColor(e, kShowWarn, Show::kWarn);
      sink.fillRounded({x, cy - 4.0f, 8.0f, 8.0f}, 4, dot);
      char ms[32];
      std::snprintf(
          ms,
          sizeof(ms),
          "%s %.1f",
          s.label.c_str(),
          static_cast<double>(s.ms)
      );
      sink.text(x + 12.0f, cy - size * 0.5f, ms, ink, size);
    }
  }
};

// Four canonical stages from the latest sample; idle placeholders
// when the hub is empty so headless renders stay deterministic.
inline std::vector<StageRow::Stage> hubStages(
    const host::TelemetryHub* hub
) {
  if (hub != nullptr && !hub->empty()) {
    host::FrameTelemetry s = hub->last();
    return {
        {"Shell", s.frameMs, 20.0f, 33.0f},
        {"Toolkit", s.buildMs, 8.0f, 16.0f},
        {"Paint", s.recordMs, 8.0f, 16.0f},
        {"GPU", s.encodeMs, 8.0f, 16.0f},
    };
  }
  return {
      {"Shell", 0.f, 20.0f, 33.0f},
      {"Toolkit", 0.f, 8.0f, 16.0f},
      {"Paint", 0.f, 8.0f, 16.0f},
      {"GPU", 0.f, 8.0f, 16.0f},
  };
}

template <class... As>
auto stageRow(StageRow r, As&&... as) {
  configureTags(r, std::forward<As>(as)...);
  return finish(std::move(r), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
