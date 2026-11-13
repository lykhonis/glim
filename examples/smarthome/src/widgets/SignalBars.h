#pragma once

// SignalBars: four rising bars, `bars` of them lit. Display only.

#include <utility>

#include "Gel.h"
#include "HomeTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct SignalBars : WithSemantic {
  static constexpr const char* kKind = "SignalBars";
  int bars = 0;  // 0..4
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(26.0f, 20.0f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, bars);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const std::uint32_t ink = themeColor(e, kThemeInkOnLight);
    const std::uint32_t lit = themeColor(e, kShowOk, Show::kOk);
    for (int i = 0; i < 4; ++i) {
      const float h = 5.0f + 5.0f * static_cast<float>(i);
      sink.fillRounded(
          {box.x + 7.0f * static_cast<float>(i),
           box.y + box.h - h,
           4.0f,
           h},
          1,
          i < bars ? lit : withAlpha(ink, 0.18f)
      );
    }
  }
};

inline Block signalBars(SignalBars s) { return finish(std::move(s)); }

}  // namespace toolkit
}  // namespace glim
