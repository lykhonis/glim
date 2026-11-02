#pragma once

#include <utility>

#include "glim/toolkit/Action.h"
#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Theme.h"

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
    rec.push(*value ? kKindOn : kKind, box, {}, semantic, 14);
    rec.push(
        kKindThumb,
        {thumbX(box), box.y + 4.0f, kThumb, kThumb},
        {},
        semantic,
        10
    );
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const Theme& t = theme(e);
    if (*value) {
      sink.fillGradient(box, 14, t.accentTop, t.accentBottom, semantic);
    } else {
      sink.fillGradient(box, 14, t.trackTop, t.trackBottom, semantic);
    }
    if (box.w > 8.0f) {
      Box line{
          detail::snapHalf(box.x + 4.0f),
          detail::snapHalf(box.y + 1.0f),
          box.w - 8.0f,
          1.0f
      };
      sink.fillRounded(line, 1, t.glossLine, semantic);
    }
    sink.stroke(box, 14, 1.0f, t.cardStroke, semantic);
    Box thumb{thumbX(box), box.y + 4.0f, kThumb, kThumb};
    Box shade{thumb.x, thumb.y + 1.0f, thumb.w, thumb.h};
    sink.fillRounded(shade, 10, t.thumbRim, semantic);
    sink.fillGradient(thumb, 10, t.thumbTop, t.thumbBottom, semantic);
    sink.stroke(thumb, 10, 1.0f, t.thumbRim, semantic);
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
