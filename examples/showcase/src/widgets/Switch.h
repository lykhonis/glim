#pragma once

#include <utility>

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

// Toggle switch: 52x28 pill, tap flips the cell.
struct Switch : WithSemantic {
  static constexpr const char* kKind = "Switch";
  static constexpr const char* kKindOn = "Switch.on";
  static constexpr const char* kKindThumb = "Switch.thumb";
  static constexpr float kW = 52.0f;
  static constexpr float kH = 28.0f;
  static constexpr float kThumb = 20.0f;
  State<bool> value;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(kW, kH, c);
  }
  float thumbX(const Box& box) const {
    return *value ? box.x + box.w - 4.0f - kThumb : box.x + 4.0f;
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
    if (*value) {
      sink.fillGradient(
          box,
          14,
          themeColor(e, kShowAccentTop, Show::kAccentTop),
          themeColor(e, kShowAccentBottom, Show::kAccentBottom)
      );
      gelCoat(sink, box, 14);
      sink.stroke(
          box,
          14,
          1.0f,
          themeColor(e, kShowCardStroke, Show::kCardStroke)
      );
    } else {
      // Groove: no stroke — the fill contrast alone shapes it.
      sink.fillGradient(
          box,
          14,
          themeColor(e, kShowTrackTop, Show::kTrackTop),
          themeColor(e, kShowTrackBottom, Show::kTrackBottom)
      );
    }
    // No 1px gloss line: it clipped the thumb crown and read as a
    // cut. The gel dome alone carries the track.
    // Elevation: soft shadow one step below, then the face, a gloss
    // crown, and a single mid-gray rim (never black — a black ring
    // around white on near-black reads as a dirty halo).
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
      return true;
    }
    return false;
  }
};

template <class... As>
Block toggle(State<bool> v, As&&... as) {
  Switch s;
  s.value = std::move(v);
  configureTags(s, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
