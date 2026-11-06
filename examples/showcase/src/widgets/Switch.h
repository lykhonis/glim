#pragma once

#include <utility>

#include "../motion/Anim.h"
#include "Gel.h"
#include "ShowcaseTheme.h"
#include "glim/toolkit/Action.h"
#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Tokens.h"
#include "glim/toolkit/theme/Keys.h"

namespace glim {
namespace toolkit {

// Toggle switch: tap flips the cell; thumb and track animate on an
// Anim. The bool stays the semantics; the Anim is only ever drawn.
struct Switch : WithSemantic {
  static constexpr const char* kKind = "Switch";
  static constexpr const char* kKindOn = "Switch.on";
  static constexpr const char* kKindThumb = "Switch.thumb";
  static constexpr float kW = 52.0f;
  static constexpr float kH = 28.0f;
  static constexpr float kThumb = 20.0f;
  State<bool> value;
  Anim<float> pos;
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
    // Rest states stay bit-identical; mid-flight cross-fades over the
    // groove.
    if (*pos < 1.0f) {
      sink.fillGradient(
          box,
          14,
          themeColor(e, kShowTrackTop, Show::kTrackTop),
          themeColor(e, kShowTrackBottom, Show::kTrackBottom)
      );
    }
    if (*pos > 0.0f) {
      const float a = *pos;
      sink.fillGradient(
          box,
          14,
          withAlpha(themeColor(e, kShowAccentTop, Show::kAccentTop), a),
          withAlpha(
              themeColor(e, kShowAccentBottom, Show::kAccentBottom), a
          )
      );
      gelCoat(sink, box, 14, a);
      sink.stroke(
          box,
          14,
          1.0f,
          withAlpha(
              themeColor(e, kShowCardStroke, Show::kCardStroke), a
          )
      );
    }
    // Elevation: shadow, face, gloss crown, single mid-gray rim (never
    // black — a black ring around white reads as a dirty halo).
    Box thumb{thumbX(box), box.y + 4.0f, kThumb, kThumb};
    Box shadow{thumb.x, thumb.y + 2.0f, thumb.w, thumb.h};
    sink.fillRounded(shadow, 10, Show::kShadow);
    sink.fillGradient(
        thumb,
        10,
        themeColor(e, kShowThumbTop, Show::kThumbTop),
        themeColor(e, kShowThumbBottom, Show::kThumbBottom)
    );
    knobGloss(sink, thumb);
    sink.stroke(
        thumb, 10, 1.0f, themeColor(e, kShowThumbRim, Show::kThumbRim)
    );
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
