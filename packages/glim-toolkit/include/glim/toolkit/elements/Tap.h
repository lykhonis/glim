#pragma once

#include <utility>
#include <vector>

#include "glim/toolkit/Action.h"
#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

// Focusable tap; touch target grows by pad.
struct Tap : WithPrimary, WithPad, WithSemantic, WithA11y {
  static constexpr const char* kKind = "Tap";
  static constexpr const char* kKindPrimary = "Tap.primary";
  const char* kind() const { return isPrimary ? kKindPrimary : kKind; }
  std::vector<Block> children;
  // Defaulted in Elements.cpp (Block is incomplete here).
  Tap();
  ~Tap();
  Tap(const Tap&);
  Tap& operator=(const Tap&);
  Tap(Tap&&);
  Tap& operator=(Tap&&);
  Action onTap;
  Box place(Constraints c, const Engine& e) const;
  void record(const Box& box, Rec& rec) const;
  void paint(const Box&, AnySink&, const Engine&) const;
  bool handle(const Event& ev) const {
    if (ev.is<PointerDown>() || ev.is<Activate>()) return detail::invokeAction(onTap);
    return false;
  }
  A11yTraits a11yTraits() const { return {.role = A11yRole::Button}; }
  Box innerBox(const Box& box) const;
  Box childBox(std::size_t, const Box& box, const Engine&) const { return innerBox(box); }
};

template <class A, class... As>
auto tap(A&& a, Action onTap, As&&... as) {
  Tap t;
  t.children.push_back(toBlock(std::forward<A>(a)));
  t.onTap = std::move(onTap);
  configureTags(t, std::forward<As>(as)...);
  return finish(std::move(t), std::forward<As>(as)...);
}

inline Box Tap::innerBox(const Box& box) const {
  float w = box.w - static_cast<float>(2 * padH);
  float h = box.h - static_cast<float>(2 * padV);
  if (w < 0) w = 0;
  if (h < 0) h = 0;
  return {box.x + static_cast<float>(padH), box.y + static_cast<float>(padV), w, h};
}

// place lives in Elements.cpp.
inline void Tap::record(const Box& box, Rec& rec) const {
  rec.push(isPrimary ? kKindPrimary : kKind, box);
}

inline void Tap::paint(const Box&, AnySink&, const Engine&) const {}

}  // namespace toolkit
}  // namespace glim
