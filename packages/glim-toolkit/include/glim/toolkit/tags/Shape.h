#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

struct RadiusTag {
  int v;
};

inline RadiusTag radius(int v) { return {v}; }

template <>
struct is_tag<RadiusTag> : std::true_type {};

struct WithRadius {
  // Parked (T4): accepted today so `card(radius(6))` keeps one shape, but
  // layout and record ignore it until glass/veil tokens land. No-op for now.
  int radius = 0;
};

template <class H>
std::enable_if_t<std::is_base_of<WithRadius, H>::value> apply(H& h, RadiusTag t) {
  h.radius = t.v;
}

}  // namespace toolkit
}  // namespace glim
