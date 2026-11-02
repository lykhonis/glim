#pragma once

#include <utility>

#include "Slider.h"
#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

// Progress bar (T9, example-local). Read-only twin of the slider
// visuals: same `Slide` track + fill kinds, no thumb, no handle —
// never focusable, never in goldens as anything but entries. Sharing
// the slider's cell demonstrates one value, two presentations.
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
