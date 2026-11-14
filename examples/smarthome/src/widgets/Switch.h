#pragma once

#include <utility>

#include "Gel.h"
#include "HomeTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Switch : WithSemantic, WithA11y {
  static constexpr const char* kKind = "Switch";
  static constexpr const char* kKindOn = "Switch.on";
  static constexpr const char* kKindThumb = "Switch.thumb";
  static constexpr float kW = 52.0f;
  static constexpr float kH = 28.0f;
  static constexpr float kThumb = 20.0f;
  State<bool> value = {"value", false};
  Anim<float> pos = {"pos", *value ? 1.0f : 0.0f};
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(kW, kH, c);
  }
  float thumbX(const Box& box) const {
    return box.x + 4.0f + (*pos) * (box.w - 8.0f - kThumb);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(*value ? kKindOn : kKind, box, {}, {}, 14);
    rec.push(
      kKindThumb,
      {thumbX(box), box.y + 4.0f, kThumb, kThumb},
      {},
      {},
      10
    );
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    // HUD switch: flat track, flat accent fill, flat thumb. No gel —
    // the only gloss left in the app is the primary button.
    if (*pos < 1.0f) {
      sink.fillRounded(
        box, 14, themeColor(e, kShowTrackTop, Show::kTrackTop)
      );
    }
    if (*pos > 0.0f) {
      const float a = *pos;
      sink.fillRounded(
        box,
        14,
        withAlpha(themeColor(e, kShowAccentTop, Show::kAccentTop), a)
      );
      sink.stroke(
        box,
        14,
        1.0f,
        withAlpha(themeColor(e, kShowCardStroke, Show::kCardStroke), a)
      );
    }
    Box thumb{thumbX(box), box.y + 4.0f, kThumb, kThumb};
    Box shadow{thumb.x, thumb.y + 2.0f, thumb.w, thumb.h};
    sink.fillRounded(shadow, 10, Show::kShadow);
    sink.fillRounded(
      thumb, 10, themeColor(e, kShowThumbTop, Show::kThumbTop)
    );
    sink.stroke(
      thumb, 10, 1.0f, themeColor(e, kShowThumbRim, Show::kThumbRim)
    );
  }
  A11yTraits a11yTraits() const {
    return {
      .role = A11yRole::Switch,
      .states = A11yStates{A11yState::Checkable}.with(
        A11yState::Checked, *value
      ),
      .actions = {A11yAction::Activate},
    };
  }
  bool handle(const Event& ev) const {
    if (ev.is<PointerDown>() || ev.is<Activate>()) {
      value = !*value;
      pos = *value ? 1.0f : 0.0f;
      return true;
    }
    return false;
  }
};

template <class... As>
Block toggle(Switch s, As&&... as) {
  configureTags(s, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
