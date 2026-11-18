#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

// The node and its children paint but take no pointer, focus or a11y:
// copies such as reflections, and decoration.
struct InertTag {};
inline InertTag inert() { return {}; }

template <>
struct is_tag<InertTag> : std::true_type {};
template <>
struct is_slot_tag<InertTag> : std::true_type {};

// Parked no-op that existing elements still inherit; inert is the working
// primitive. Remove once no element derives from it.
struct ThroughTag {};
inline ThroughTag through() { return {}; }
template <>
struct is_tag<ThroughTag> : std::true_type {};
struct WithThrough {
  bool through = false;
};
template <class H>
std::enable_if_t<std::is_base_of<WithThrough, H>::value> apply(H& h, ThroughTag) {
  h.through = true;
}

}  // namespace toolkit
}  // namespace glim
