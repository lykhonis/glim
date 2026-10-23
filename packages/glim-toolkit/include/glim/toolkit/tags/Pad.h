#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

struct PadTag {
  int h;
  int v;
};

inline PadTag pad(int v) { return {v, v}; }
inline PadTag pad(int h, int v) { return {h, v}; }

template <>
struct is_tag<PadTag> : std::true_type {};

struct WithPad {
  int padH = 0;
  int padV = 0;
};

template <class H>
std::enable_if_t<std::is_base_of<WithPad, H>::value> apply(H& h, PadTag t) {
  h.padH = t.h;
  h.padV = t.v;
}

}  // namespace toolkit
}  // namespace glim
