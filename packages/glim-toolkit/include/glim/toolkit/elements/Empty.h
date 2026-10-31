#pragma once

#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

struct Empty {
  static constexpr const char* kKind = "Empty";
  const char* kind() const { return kKind; }
  Box place(Constraints c, const Engine&) const {
    return {0, 0, detail::clampF(0.0f, c.minW, c.maxW), detail::clampF(0.0f, c.minH, c.maxH)};
  }
  void record(const Box& box, Rec& rec, const Engine&) const { rec.push(kKind, box); }
};

}  // namespace toolkit
}  // namespace glim
