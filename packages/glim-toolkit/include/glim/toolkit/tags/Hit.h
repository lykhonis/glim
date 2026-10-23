#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

struct ThroughTag {};

inline ThroughTag through() { return {}; }

template <>
struct is_tag<ThroughTag> : std::true_type {};

struct WithThrough {
  // Parked: accepted today, but hit-testing always walks top-down last-wins
  // (see Place.h) until the hit-through proposal (§11) lands. No-op for now.
  bool through = false;
};

template <class H>
std::enable_if_t<std::is_base_of<WithThrough, H>::value> apply(H& h, ThroughTag) {
  h.through = true;
}

}  // namespace toolkit
}  // namespace glim
