#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

// Opt-in cross-axis centering for `Row` (vertical) and `Column`
// (horizontal) — the dashboard look (icons, labels and buttons sharing
// a row at different heights; stat columns centering under each
// other). Off by default: the catalog's left/top alignment is
// existing behavior and stays put.
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
