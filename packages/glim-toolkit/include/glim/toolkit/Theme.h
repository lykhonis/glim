#pragma once

// Subtree theme scope (values-DSL spirit). The element owns its Env
// slot lifetime internally: place/childBox push the entries on the
// stack for layout, the walk pushes them for record/paint/hit/focus
// (see Walk.h). Callers never retain slots. Siblings are isolated by
// construction (each scope is its own Env node); nested scopes
// compose inner-wins via chain order.
//
// Layout is a gapless column (transparent stacking); theme never
// affects geometry, so any container shape would do — column reads
// naturally for a themed section.

#include <cstddef>
#include <cstdint>
#include <typeinfo>
#include <utility>
#include <vector>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/theme/Keys.h"

namespace glim {
namespace toolkit {

inline constexpr std::size_t kMaxThemeEntries = 8;

struct Themed {
  static constexpr const char* kKind = "Themed";
  const char* kind() const { return kKind; }
  ThemeEntry entries[kMaxThemeEntries]{};
  std::size_t count = 0;
  std::vector<Block> children;
  ThemeScope themeScope() const { return {entries, count}; }
  Box place(Constraints c, const Engine& e) const {
    ThemeScope s{entries, count};
    Env node{&s, &typeid(ThemeScope), e.env};
    Engine de = e;
    de.env = &node;
    return detail::flowColumn(children, 0, c, de).size;
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    ThemeScope s{entries, count};
    Env node{&s, &typeid(ThemeScope), e.env};
    Engine de = e;
    de.env = &node;
    detail::Flow f = detail::flowColumn(children, 0, {0, box.w, 0, box.h}, de);
    Box b = f.children[i];
    b.x += box.x;
    b.y += box.y;
    return b;
  }
};

// Named builders for the toolkit-owned keys (values-DSL, no builder
// object): themed(windowBg(...), lines("hi")). Custom keys go through
// themeColor/themeFloat/themeInt directly. Later entries shadow
// earlier ones within one scope.
inline ThemeEntry windowBg(std::uint32_t rgba) { return themeColor(kThemeWindowBg, rgba); }
inline ThemeEntry inkOnLight(std::uint32_t rgba) { return themeColor(kThemeInkOnLight, rgba); }
inline ThemeEntry focusRing(std::uint32_t rgba) { return themeColor(kThemeFocusRing, rgba); }
inline ThemeEntry typeLine(float v) { return themeFloat(kThemeTypeLine, v); }

}  // namespace toolkit
}  // namespace glim
