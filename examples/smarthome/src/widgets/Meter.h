#pragma once

// Meter: a thick bar filled to `value`, the unfilled rest hatched.
// Display only; spans the width it is given.

#include <utility>

#include "Gel.h"
#include "HomeTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Meter : WithSemantic {
  static constexpr const char* kKind = "Meter";
  static constexpr float kH = 40.0f;
  float value = 0.0f;  // 0..1
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(
      isUnbounded(c.maxW) ? 240.0f : c.maxW, kH, c
    );
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 12);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const std::uint32_t ink = themeColor(e, kThemeInkOnLight);
    sink.fillRounded(box, 12, withAlpha(ink, 0.06f));
    const float fillW = box.w * value;
    for (float x = box.x + fillW + 6.0f; x < box.x + box.w - 6.0f;
         x += 5.0f)
      sink.fillRounded(
        {x, box.y + 6.0f, 1.5f, box.h - 12.0f}, 1, withAlpha(ink, 0.18f)
      );
    if (fillW < 1.0f) return;
    const Box fill{box.x, box.y, fillW, box.h};
    sink.fillGradient(
      fill,
      12,
      themeColor(e, kShowAccentTop, Show::kAccentTop),
      themeColor(e, kShowAccentBottom, Show::kAccentBottom)
    );
    gelCoat(sink, fill, 12);
  }
};

inline Block meter(Meter m) { return finish(std::move(m)); }

}  // namespace toolkit
}  // namespace glim
