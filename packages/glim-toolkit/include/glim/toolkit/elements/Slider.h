#pragma once

#include <utility>

#include "glim/toolkit/A11yTraits.h"
#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Event.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/tags/A11y.h"
#include "glim/toolkit/tags/Semantic.h"

namespace glim {
namespace toolkit {

// A value in lo..hi on a horizontal track: press or drag to set it, Left and
// Right or a screen reader's adjust to step it. Values land on whole steps.
struct Slider : WithSemantic, WithA11y {
  static constexpr const char* kKind = "Slider";
  static constexpr float kHeight = 24.0f;
  static constexpr float kKnob = 18.0f;
  const char* kind() const { return kKind; }
  State<float> value;
  float lo = 0.0f;
  float hi = 1.0f;
  float step = 0.0f;
  float width = 200.0f;
  Box place(Constraints c, const Engine&) const { return detail::resolveSize(width, kHeight, c); }
  void paint(const Box& box, AnySink& sink, const Engine& e) const;
  bool handleAt(const Box& box, const Event& ev) const;
  bool handle(const Event& ev) const;
  A11yTraits a11yTraits() const {
    return {
      .role = A11yRole::Slider,
      .range = A11yRange{*value, lo, hi},
      .actions = {A11yAction::Increment, A11yAction::Decrement},
    };
  }
  // The track position x maps to, snapped and clamped.
  float at(const Box& box, float x) const;
  float stepped(float v, int steps) const;
};

template <class... As>
auto slider(Slider s, As&&... as) {
  configureTags(s, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
