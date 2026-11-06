#pragma once

// Fixed-size breathing block: exact clearance with no paint,
// no focus, no behavior.

#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Strut {
  static constexpr const char* kKind = "Strut";
  const char* kind() const { return kKind; }
  float width = 0.0f;
  float height = 0.0f;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(width, height, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {});
  }
};

template <class... As>
Block strut(Strut s, As&&... as) {
  configureTags(s, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
