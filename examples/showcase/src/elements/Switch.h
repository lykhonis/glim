#pragma once

#include <utility>

#include "glim/toolkit/Action.h"
#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

// Toggle switch (T8 catalog, example-local). Fixed 52x28 pill; the
// kinds below are interpreted by the showcase sink (track navy/teal,
// white thumb) — zero codec edits. Tapping anywhere flips the cell.
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
  void record(const Box& box, Rec& rec) const {
    rec.push(*value ? kKindOn : kKind, box, {}, semantic, 14);
    float tx = *value ? box.x + box.w - 4.0f - kThumb : box.x + 4.0f;
    rec.push(
        kKindThumb, {tx, box.y + 4.0f, kThumb, kThumb}, {}, semantic, 10
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
