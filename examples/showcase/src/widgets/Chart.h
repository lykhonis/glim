#pragma once

// Bar chart: synthetic series or telemetry history as rounded bars over
// a baseline. Read-only, never focusable. Values are 0..1 fractions.

#include <algorithm>
#include <utility>
#include <vector>

#include "Gel.h"
#include "ShowcaseTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Bars {
  static constexpr const char* kKind = "Bars";
  std::vector<float> values;
  float height = 120.0f;
  Box place(Constraints c, const Engine&) const {
    float w = values.empty()
                  ? 120.0f
                  : static_cast<float>(values.size()) * 14.0f;
    return detail::resolveSize(w, height, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 4);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    if (values.empty()) return;
    const float n = static_cast<float>(values.size());
    const float slot = box.w / n;
    const float bw = slot > 4.0f ? slot - 4.0f : slot;
    for (std::size_t i = 0; i < values.size(); ++i) {
      float v = values[i];
      if (v < 0.f) v = 0.f;
      if (v > 1.f) v = 1.f;
      const float h = 4.0f + v * (box.h - 12.0f);
      Box bar{
          box.x + static_cast<float>(i) * slot + 2.0f,
          box.y + box.h - 6.0f - h,
          bw,
          h
      };
      sink.fillGradient(
          bar,
          3,
          themeColor(e, kShowAccentTop, Show::kAccentTop),
          themeColor(e, kShowAccentBottom, Show::kAccentBottom)
      );
    }
    Box base{box.x + 6.0f, box.y + box.h - 6.0f, box.w - 12.0f, 1.0f};
    sink.fillRounded(
        base, 1, themeColor(e, kShowCardStroke, Show::kCardStroke)
    );
  }
};

template <class... As>
Block bars(Bars b, As&&... as) {
  configureTags(b, std::forward<As>(as)...);
  return finish(std::move(b), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
