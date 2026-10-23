#pragma once

#include <type_traits>

#include "glim/toolkit/State.h"
#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {

struct GapTag {
  int v;
};

inline GapTag gap(int v) { return {v}; }

template <>
struct is_tag<GapTag> : std::true_type {};

struct WithGap {
  int gap = 0;
};

template <class H>
std::enable_if_t<std::is_base_of<WithGap, H>::value> apply(H& h, GapTag t) {
  int v = t.v < 0 ? 0 : (t.v > Tokens::kGapMax ? Tokens::kGapMax : t.v);
  h.gap = v;
}

}  // namespace toolkit
}  // namespace glim
