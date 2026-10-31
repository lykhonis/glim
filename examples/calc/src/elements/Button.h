#pragma once

#include <string>
#include <utility>

#include "glim/toolkit/Action.h"
#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {

struct Button : WithGap, WithPrimary, WithThrough, WithPad, WithSemantic {
  static constexpr const char* kKind = "Button";
  static constexpr const char* kKindPrimary = "Button.primary";
  std::string label;
  Action onTap;
  Box place(Constraints c, const Engine&) const {
    Box t = detail::measureText(label, {0, kUnbounded, 0, kUnbounded});
    return detail::resolveSize(t.w + static_cast<float>(2 * padH),
                               t.h + static_cast<float>(2 * padV), c);
  }
  void record(const Box& box, Rec& rec, const Engine&) const {
    rec.push(isPrimary ? kKindPrimary : kKind, box, label, semantic);
  }
  bool handle(const Event& ev) const {
    if (ev.is<PointerDown>() || ev.is<Activate>()) return detail::invokeAction(onTap);
    return false;
  }
};

template <class... As>
Block button(std::string label, Action onTap, As&&... as) {
  Button b;
  b.label = std::move(label);
  b.onTap = std::move(onTap);
  b.padH = Tokens::kButtonPadH;
  b.padV = Tokens::kButtonPadV;
  configureTags(b, std::forward<As>(as)...);
  return Block{Custom{std::move(b)}};
}

}  // namespace toolkit
}  // namespace glim
