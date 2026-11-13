#pragma once

#include <type_traits>
#include <utility>

#include "glim/toolkit/A11yTraits.h"

namespace glim {
namespace toolkit {

struct A11yTag {
  A11yTraits traits;
};

inline A11yTag a11y(A11yTraits traits) { return {std::move(traits)}; }

template <>
struct is_tag<A11yTag> : std::true_type {};

struct WithA11y {
  A11yTraits a11y;
};

template <class H>
std::enable_if_t<std::is_base_of<WithA11y, H>::value> apply(H& h, A11yTag t) {
  h.a11y = std::move(t.traits);
}

}  // namespace toolkit
}  // namespace glim
