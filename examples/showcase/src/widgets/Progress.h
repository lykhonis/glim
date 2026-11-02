#pragma once

#include <utility>

#include "Slider.h"
#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Theme.h"

namespace glim {
namespace toolkit {

// Progress bar: read-only slider twin, shares the cell.
struct Progress : WithSemantic {
  static constexpr float kW = 240.0f;
  static constexpr float kH = 12.0f;
  State<float> value;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(kW, kH, c);
  }
  void record(const Box& box, Rec& rec) const {
    float v = Slider::clamp01(*value);
    rec.push(Slider::kKind, box, {}, semantic, 6);
    rec.push(
        Slider::kKindFill,
        {box.x, box.y, box.w * v, box.h},
        {},
        semantic,
        6
    );
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const Theme& t = theme(e);
    float v = Slider::clamp01(*value);
    sink.fillGradient(box, 6, t.trackTop, t.trackBottom, semantic);
    if (box.w * v > 0.5f) {
      Box fill{box.x, box.y, box.w * v, box.h};
      sink.fillGradient(fill, 6, t.accentTop, t.accentBottom, semantic);
      if (fill.w > 10.0f) {
        Box sheen{
            detail::snapHalf(fill.x + 2.0f),
            detail::snapHalf(fill.y + 1.0f),
            fill.w - 4.0f,
            1.0f
        };
        sink.fillRounded(sheen, 1, t.glossLine, semantic);
      }
    }
    sink.stroke(box, 6, 1.0f, t.cardStroke, semantic);
  }
};

template <class... As>
Block progress(State<float> v, As&&... as) {
  Progress p;
  p.value = std::move(v);
  configureTags(p, std::forward<As>(as)...);
  return finish(std::move(p), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
