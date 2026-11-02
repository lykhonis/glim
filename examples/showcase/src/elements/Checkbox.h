#pragma once

#include <string>
#include <utility>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {

// Labeled checkbox (T8 catalog, example-local). 24px box + label;
// the kinds below are interpreted by the showcase sink (hollow navy
// when off, solid teal when on) — zero codec edits. Tapping anywhere
// flips the cell.
struct Checkbox : WithSemantic {
  static constexpr const char* kKind = "Check";
  static constexpr const char* kKindOn = "Check.on";
  static constexpr float kBox = 24.0f;
  State<bool> value;
  std::string label;
  Box place(Constraints c, const Engine& e) const {
    Box t =
        detail::measureText(label, {0, kUnbounded, 0, kUnbounded}, e);
    return detail::resolveSize(t.w + kBox + 8.0f, kBox, c);
  }
  void record(const Box& box, Rec& rec) const {
    float by = box.y + (box.h - kBox) * 0.5f;
    rec.push(
        *value ? kKindOn : kKind,
        {box.x, by, kBox, kBox},
        {},
        semantic,
        6
    );
    rec.push(
        "Lines",
        {box.x + kBox + 8.0f,
         by + (kBox - Tokens::kTypeLine) * 0.5f,
         box.w - kBox - 8.0f,
         Tokens::kTypeLine},
        label,
        semantic
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
Block checkbox(std::string label, State<bool> v, As&&... as) {
  Checkbox c;
  c.label = std::move(label);
  c.value = std::move(v);
  configureTags(c, std::forward<As>(as)...);
  return finish(std::move(c), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
