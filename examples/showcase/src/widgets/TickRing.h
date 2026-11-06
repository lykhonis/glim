#pragma once

// TickRing: restrained HUD gauge. 24 trig-placed tick fills around a
// circle; lit count ∝ value 0..1. Donut center is circle-over-circle
// (fillRounded square at full radius). Read-only, never focusable.
#include <cmath>
#include <string>
#include <utility>

#include "ShowcaseTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct TickRing {
  static constexpr const char* kKind = "TickRing";
  static constexpr int kTicks = 24;
  float value = 0.f;  // 0..1
  float diameter = 96.0f;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(diameter, diameter, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, static_cast<int>(box.w / 2));
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    float v = value;
    if (v < 0.f) v = 0.f;
    if (v > 1.f) v = 1.f;
    const float cx = box.x + box.w * 0.5f;
    const float cy = box.y + box.h * 0.5f;
    const float r = (box.w < box.h ? box.w : box.h) * 0.5f - 3.0f;
    const std::uint32_t lit =
        themeColor(e, kShowAccentTop, Show::kAccentTop);
    const std::uint32_t dim =
        themeColor(e, kShowCardStroke, Show::kCardStroke);
    const int litCount = static_cast<int>(v * kTicks + 0.5f);
    for (int i = 0; i < kTicks; ++i) {
      const float a =
          static_cast<float>(i) / kTicks * 2.0f * 3.14159265f;
      const float c = std::cos(a), s = std::sin(a);
      const float r0 = r - 7.0f, r1 = r - 2.0f;
      Box tick{cx + c * r0 - 1.0f, cy + s * r0 - 1.0f, 2.0f, 2.0f};
      // Radial tick: stretch along the radius for lit, dot for dim.
      if (i < litCount) {
        tick = Box{cx + c * r1 - 1.5f, cy + s * r1 - 1.5f, 3.0f, 3.0f};
        sink.fillRounded(tick, 1, lit);
      } else {
        sink.fillRounded(tick, 1, dim);
      }
    }
    // Donut hole: card-bottom circle over the tick bed.
    const float inner = r - 12.0f;
    Box hole{cx - inner, cy - inner, inner * 2.0f, inner * 2.0f};
    sink.fillRounded(
        hole,
        static_cast<int>(inner),
        themeColor(e, kShowCardBottom, Show::kCardBottom)
    );
  }
};

template <class... As>
auto tickRing(TickRing t, As&&... as) {
  configureTags(t, std::forward<As>(as)...);
  return finish(std::move(t), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
