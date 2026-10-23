#pragma once

#include <type_traits>

#include "glim/toolkit/State.h"

namespace glim {
namespace toolkit {

struct ScrollOffsetTag {
  State<float> h;
};

inline ScrollOffsetTag scrollOffset(State<float> h) { return {h}; }

template <>
struct is_tag<ScrollOffsetTag> : std::true_type {};

struct WithScrollOffset {
  State<float> scrollOff;
};

template <class H>
std::enable_if_t<std::is_base_of<WithScrollOffset, H>::value> apply(H& h, ScrollOffsetTag t) {
  h.scrollOff = t.h;
}

}  // namespace toolkit
}  // namespace glim
