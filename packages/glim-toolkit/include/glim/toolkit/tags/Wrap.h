#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

// Opt-in paragraph wrap; without it Lines spills single-line.
struct WrapTag {};

inline WrapTag wrap() { return {}; }

template <>
struct is_tag<WrapTag> : std::true_type {};

struct WithWrap {
  bool wrap = false;
};

template <class H>
std::enable_if_t<std::is_base_of<WithWrap, H>::value> apply(H& h, WrapTag) {
  h.wrap = true;
}

}  // namespace toolkit
}  // namespace glim
