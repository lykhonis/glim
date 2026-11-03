#pragma once

#include <utility>

#include "Gel.h"
#include "Slider.h"
#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/theme/Keys.h"

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
    float v = Slider::clamp01(*value);
    // Groove: no stroke — the fill contrast alone shapes it.
    sink.fillGradient(
        box,
        6,
        themeColor(e, kThemeTrackTop, Tokens::kTrackTop),
        themeColor(e, kThemeTrackBottom, Tokens::kTrackBottom),
        semantic
    );
    if (box.w * v > 0.5f) {
      Box fill{box.x, box.y, box.w * v, box.h};
      // Square the leading cap: it hides under nothing here, so a
      // round cap would bulge past the value. Paint the fill body
      // then cap the start round only via the track beneath.
      sink.fillGradient(
          fill,
          6,
          themeColor(e, kThemeAccentTop, Tokens::kAccentTop),
          themeColor(e, kThemeAccentBottom, Tokens::kAccentBottom),
          semantic
      );
      gelCoat(sink, fill, 6, semantic);
    }
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
