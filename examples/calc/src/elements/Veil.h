#pragma once

#include <utility>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

// Modal scrim: fills its allotted box, paints as a translucent fill.
// Calc-local (same composition proof as Button/Card); toolkit core
// stays paint-free. In a Stack it rides as children[0] under the
// dialog.
struct Veil {
  static constexpr const char* kKind = "Veil";
  const char* kind() const { return kKind; }
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(
        isUnbounded(c.maxW) ? 0.0f : c.maxW,
        isUnbounded(c.maxH) ? 0.0f : c.maxH,
        c
    );
  }
  void record(const Box& box, Rec& rec, const Engine&) const {
    rec.push(kKind, box);
  }
};

template <class... As>
Block veil(As&&... as) {
  Veil v;
  configureTags(v, std::forward<As>(as)...);
  return finish(std::move(v), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
