#pragma once

#include <utility>
#include <vector>

#include "glim/toolkit/Action.h"
#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

// Focusable tappable. Owns the action; lays out its single child plus pad
// chrome (touch target), records a "Tap" wrapper, fires onTap.
struct Tap : WithPrimary, WithPad, WithSemantic {
  static constexpr const char* kKind = "Tap";
  static constexpr const char* kKindPrimary = "Tap.primary";
  std::vector<Block> children;
  Action onTap;
  Box place(Constraints c, const Engine& e) const;
  void record(const Box& box, Rec& rec, const Engine& e) const;
  bool fire() const { return detail::fireAction(onTap); }
  Box innerBox(const Box& box) const;
};

template <class A, class... As>
Tap tap(A&& a, Action onTap, As&&... as) {
  Tap t;
  t.children.push_back(toBlock(std::forward<A>(a)));
  t.onTap = std::move(onTap);
  configureTags(t, std::forward<As>(as)...);
  return t;
}

inline Box Tap::innerBox(const Box& box) const {
  float w = box.w - static_cast<float>(2 * padH);
  float h = box.h - static_cast<float>(2 * padV);
  if (w < 0) w = 0;
  if (h < 0) h = 0;
  return {box.x + static_cast<float>(padH), box.y + static_cast<float>(padV), w, h};
}

inline Box Tap::place(Constraints c, const Engine& e) const {
  Constraints ic = detail::inset(c, static_cast<float>(2 * padH), static_cast<float>(2 * padV));
  Box inner = e.place(children[0], ic);
  return detail::resolveSize(inner.w + static_cast<float>(2 * padH),
                             inner.h + static_cast<float>(2 * padV), c);
}

inline void Tap::record(const Box& box, Rec& rec, const Engine& e) const {
  rec.push(isPrimary ? kKindPrimary : kKind, box, {}, semantic);
  e.record(children[0], innerBox(box), rec);
}

}  // namespace toolkit
}  // namespace glim
