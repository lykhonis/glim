#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

// Opt-in paragraph wrap for Lines (T3). Without it Lines is a single line
// that spills past narrow budgets (T1 overflow lock). With wrap() Lines
// greedy-wraps at maxW and ellipsizes to maxH ("..." suffix).
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
