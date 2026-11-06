#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

// Opt-in cross-axis centering; off by default.
struct CenterTag {};

inline CenterTag center() { return {}; }

template <>
struct is_tag<CenterTag> : std::true_type {};

struct WithCenter {
  bool centered = false;
};

template <class H>
std::enable_if_t<std::is_base_of<WithCenter, H>::value> apply(H& h, CenterTag) {
  h.centered = true;
}

}  // namespace toolkit
}  // namespace glim
