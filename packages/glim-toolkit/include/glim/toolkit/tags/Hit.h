#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

struct ThroughTag {};

inline ThroughTag through() { return {}; }

template <>
struct is_tag<ThroughTag> : std::true_type {};

struct WithThrough {
  // Parked: no-op until hit-through lands.
  bool through = false;
};

template <class H>
std::enable_if_t<std::is_base_of<WithThrough, H>::value> apply(H& h, ThroughTag) {
  h.through = true;
}

}  // namespace toolkit
}  // namespace glim
