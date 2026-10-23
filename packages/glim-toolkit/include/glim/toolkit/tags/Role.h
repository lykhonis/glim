#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

struct PrimaryTag {};

inline PrimaryTag primary() { return {}; }

template <>
struct is_tag<PrimaryTag> : std::true_type {};

struct WithPrimary {
  bool isPrimary = false;
};

template <class H>
std::enable_if_t<std::is_base_of<WithPrimary, H>::value> apply(H& h, PrimaryTag) {
  h.isPrimary = true;
}

} // namespace toolkit
}  // namespace glim
