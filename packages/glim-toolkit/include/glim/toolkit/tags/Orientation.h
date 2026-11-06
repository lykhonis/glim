#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

struct HorizontalTag {};
struct VerticalTag {};

inline HorizontalTag horizontal() { return {}; }
inline VerticalTag vertical() { return {}; }

template <>
struct is_tag<HorizontalTag> : std::true_type {};
template <>
struct is_tag<VerticalTag> : std::true_type {};

struct WithAxis {
  int axis = 0;
};

constexpr int kAxisVertical = 0;
constexpr int kAxisHorizontal = 1;

template <class H>
std::enable_if_t<std::is_base_of<WithAxis, H>::value> apply(H& h, HorizontalTag) {
  h.axis = kAxisHorizontal;
}
template <class H>
std::enable_if_t<std::is_base_of<WithAxis, H>::value> apply(H& h, VerticalTag) {
  h.axis = kAxisVertical;
}

}  // namespace toolkit
}  // namespace glim
