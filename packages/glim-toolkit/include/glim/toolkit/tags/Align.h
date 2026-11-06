#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

struct AlignLeftTag {};
struct AlignCenterTag {};
struct AlignRightTag {};

inline AlignLeftTag alignLeft() { return {}; }
inline AlignCenterTag alignCenter() { return {}; }
inline AlignRightTag alignRight() { return {}; }

template <>
struct is_tag<AlignLeftTag> : std::true_type {};
template <>
struct is_tag<AlignCenterTag> : std::true_type {};
template <>
struct is_tag<AlignRightTag> : std::true_type {};

struct WithAlign {
  int align = 0;
};

constexpr int kAlignLeft = 0;
constexpr int kAlignCenter = 1;
constexpr int kAlignRight = 2;

template <class H>
std::enable_if_t<std::is_base_of<WithAlign, H>::value> apply(H& h, AlignLeftTag) {
  h.align = kAlignLeft;
}
template <class H>
std::enable_if_t<std::is_base_of<WithAlign, H>::value> apply(H& h, AlignCenterTag) {
  h.align = kAlignCenter;
}
template <class H>
std::enable_if_t<std::is_base_of<WithAlign, H>::value> apply(H& h, AlignRightTag) {
  h.align = kAlignRight;
}

}  // namespace toolkit
}  // namespace glim
