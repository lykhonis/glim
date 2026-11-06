#pragma once

#include <utility>

#include "Gel.h"
#include "ShowcaseTheme.h"
#include "Slider.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Progress : WithSemantic {
  static constexpr float kW = 240.0f;
  static constexpr float kH = 12.0f;
  State<float> value = {"value", 0.4f};
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(kW, kH, c);
  }
  void record(const Box& box, Rec& rec) const {
    float v = Slider::clamp01(*value);
    rec.push(Slider::kKind, box, {}, {}, 6);
    rec.push(
        Slider::kKindFill, {box.x, box.y, box.w * v, box.h}, {}, {}, 6
    );
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    float v = Slider::clamp01(*value);
    sink.fillGradient(
        box,
        6,
        themeColor(e, kShowTrackTop, Show::kTrackTop),
        themeColor(e, kShowTrackBottom, Show::kTrackBottom)
    );
    if (box.w * v > 0.5f) {
      Box fill{box.x, box.y, box.w * v, box.h};
      sink.fillGradient(
          fill,
          6,
          themeColor(e, kShowAccentTop, Show::kAccentTop),
          themeColor(e, kShowAccentBottom, Show::kAccentBottom)
      );
      gelCoat(sink, fill, 6);
    }
  }
};

template <class... As>
Block progress(Progress p, As&&... as) {
  configureTags(p, std::forward<As>(as)...);
  return finish(std::move(p), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
