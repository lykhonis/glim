#pragma once

// Subtree theme scope; theme never affects geometry.
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

struct Themed {
  static constexpr const char* kKind = "Themed";
  const char* kind() const { return kKind; }
  ThemeEntry entries[kMaxThemeEntries]{};
  std::size_t count = 0;
  std::vector<Block> children;
  ThemeScope themeScope() const {
    ThemeScope s;
    for (std::size_t i = 0; i < count; ++i) s.entries[i] = entries[i];
    s.count = count;
    return s;
  }
  Box place(Constraints c, const Engine& e) const {
    ThemeScope s = themeScope();
    Env node{&s, &typeid(ThemeScope), e.env};
    Engine de = e;
    de.env = node;
    return detail::flowColumn(children, 0, c, de).size;
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    ThemeScope s = themeScope();
    Env node{&s, &typeid(ThemeScope), e.env};
    Engine de = e;
    de.env = node;
    detail::Flow f = detail::flowColumn(children, 0, {0, box.w, 0, box.h}, de);
    Box b = f.children[i];
    b.x += box.x;
    b.y += box.y;
    return b;
  }
};

// Named builders for toolkit-owned keys; later entries shadow earlier ones.
inline ThemeEntry windowBg(std::uint32_t rgba) { return themeColor(kThemeWindowBg, rgba); }
inline ThemeEntry inkOnLight(std::uint32_t rgba) { return themeColor(kThemeInkOnLight, rgba); }
inline ThemeEntry focusRing(std::uint32_t rgba) { return themeColor(kThemeFocusRing, rgba); }
inline ThemeEntry typeLine(float v) { return themeFloat(kThemeTypeLine, v); }

}  // namespace toolkit
}  // namespace glim
