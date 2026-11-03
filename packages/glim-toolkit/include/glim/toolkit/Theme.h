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

// Named builders for the builtin ramp (values-DSL, no builder
// object): themed(accent(0xff5a1fff), lines("hi")). Custom keys go
// through themeColor/themeFloat/themeInt directly. Later entries
// shadow earlier ones within one scope.
inline ThemeEntry windowBg(std::uint32_t rgba) { return themeColor(kThemeWindowBg, rgba); }
inline ThemeEntry cardFill(std::uint32_t rgba) { return themeColor(kThemeCardFill, rgba); }
inline ThemeEntry cardStroke(std::uint32_t rgba) { return themeColor(kThemeCardStroke, rgba); }
inline ThemeEntry buttonFill(std::uint32_t rgba) { return themeColor(kThemeButtonFill, rgba); }
inline ThemeEntry buttonPrimaryTop(std::uint32_t rgba) {
  return themeColor(kThemeButtonPrimaryTop, rgba);
}
inline ThemeEntry buttonPrimaryBottom(std::uint32_t rgba) {
  return themeColor(kThemeButtonPrimaryBottom, rgba);
}
inline ThemeEntry accent(std::uint32_t rgba) { return themeColor(kThemeAccent, rgba); }
inline ThemeEntry inkOnLight(std::uint32_t rgba) { return themeColor(kThemeInkOnLight, rgba); }
inline ThemeEntry inkSecondary(std::uint32_t rgba) { return themeColor(kThemeInkSecondary, rgba); }
inline ThemeEntry hairline(std::uint32_t rgba) { return themeColor(kThemeHairline, rgba); }
inline ThemeEntry focusRing(std::uint32_t rgba) { return themeColor(kThemeFocusRing, rgba); }
inline ThemeEntry layerFallback(std::uint32_t rgba) {
  return themeColor(kThemeLayerFallback, rgba);
}
inline ThemeEntry typeLine(float v) { return themeFloat(kThemeTypeLine, v); }
inline ThemeEntry typeDisplay(float v) { return themeFloat(kThemeTypeDisplay, v); }
inline ThemeEntry glassThickness(float v) { return themeFloat(kThemeGlassThickness, v); }
inline ThemeEntry buttonRadius(int v) { return themeInt(kThemeButtonRadius, v); }
inline ThemeEntry glossLine(std::uint32_t rgba) { return themeColor(kThemeGlossLine, rgba); }
inline ThemeEntry thumbRim(std::uint32_t rgba) { return themeColor(kThemeThumbRim, rgba); }
inline ThemeEntry glossSheen(std::uint32_t rgba) { return themeColor(kThemeGlossSheen, rgba); }
inline ThemeEntry cardTop(std::uint32_t rgba) { return themeColor(kThemeCardTop, rgba); }
inline ThemeEntry cardBottom(std::uint32_t rgba) { return themeColor(kThemeCardBottom, rgba); }
inline ThemeEntry buttonSecondaryTop(std::uint32_t rgba) {
  return themeColor(kThemeButtonSecondaryTop, rgba);
}
inline ThemeEntry buttonSecondaryBottom(std::uint32_t rgba) {
  return themeColor(kThemeButtonSecondaryBottom, rgba);
}
inline ThemeEntry thumbTop(std::uint32_t rgba) { return themeColor(kThemeThumbTop, rgba); }
inline ThemeEntry thumbBottom(std::uint32_t rgba) { return themeColor(kThemeThumbBottom, rgba); }
inline ThemeEntry trackTop(std::uint32_t rgba) { return themeColor(kThemeTrackTop, rgba); }
inline ThemeEntry trackBottom(std::uint32_t rgba) { return themeColor(kThemeTrackBottom, rgba); }
inline ThemeEntry accentTop(std::uint32_t rgba) { return themeColor(kThemeAccentTop, rgba); }
inline ThemeEntry accentBottom(std::uint32_t rgba) { return themeColor(kThemeAccentBottom, rgba); }
inline ThemeEntry gelGloss(std::uint32_t rgba) { return themeColor(kThemeGelGloss, rgba); }
inline ThemeEntry gelEdge(std::uint32_t rgba) { return themeColor(kThemeGelEdge, rgba); }

}  // namespace toolkit
}  // namespace glim
