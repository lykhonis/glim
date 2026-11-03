#pragma once

// themed() factory: partitions ThemeEntry args (stored inline, later
// shadows earlier) from children, leaving slot tags for finish to
// wrap — so themed(accent(...), expand(), child) keeps its scope
// through the Tagged wrapper (see Tagged::themeScope).

#include <cassert>
#include <type_traits>
#include <utility>
#include <vector>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Theme.h"

namespace glim {
namespace toolkit {

template <class... As>
Block themed(As&&... as) {
  Themed t;
  std::vector<Block> kids;
  auto grab = [&](auto&& a) {
    using T = std::decay_t<decltype(a)>;
    if constexpr (std::is_same<T, ThemeEntry>::value) {
      assert(t.count < kMaxThemeEntries);
      if (t.count < kMaxThemeEntries) t.entries[t.count++] = a;
    } else if constexpr (!is_slot_tag<T>::value) {
      kids.push_back(toBlock(std::forward<decltype(a)>(a)));
    }
  };
  (grab(std::forward<As>(as)), ...);
  t.children = std::move(kids);
  return finish(std::move(t), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
