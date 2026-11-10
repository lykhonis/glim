#pragma once

// Scaled text: same monospace metrics as Lines, scaled by size/14 so
// measure and paint agree. Titles (~20), KPI values (~28) and heroes
// (~56) read from theme keys; body text stays on Lines.
#include <string>
#include <utility>

#include "HomeTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct ScaledLines : WithAlign, WithSemantic {
  static constexpr const char* kKind = "Lines.scale";
  std::string text;
  ThemeKey sizeKey{kThemeTypeLine};
  float fallback = Tokens::kTypeLine;
  Box place(Constraints c, const Engine& e) const {
    const float size = themeFloat(e, sizeKey, fallback);
    const float k = size / Tokens::kTypeLine;
    float w =
        static_cast<float>(text.size()) * Tokens::kTypeAdvance * k;
    return detail::resolveSize(w, size, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, text);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const float size = themeFloat(e, sizeKey, fallback);
    const float k = size / Tokens::kTypeLine;
    const std::uint32_t ink =
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight);
    float lw =
        static_cast<float>(text.size()) * Tokens::kTypeAdvance * k;
    float x = box.x;
    if (align == kAlignCenter) x += (box.w - lw) * 0.5f;
    if (align == kAlignRight) x += box.w - lw;
    sink.text(x, box.y, text, ink, size);
  }
};

template <class... As>
auto scaledLines(ScaledLines l, As&&... as) {
  configureTags(l, std::forward<As>(as)...);
  return finish(std::move(l), std::forward<As>(as)...);
}

inline Block titleLines(std::string text) {
  ScaledLines l;
  l.text = std::move(text);
  l.sizeKey = kShowTitleSize;
  l.fallback = Show::kTitleSize;
  return scaledLines(std::move(l));
}

inline Block valueLines(std::string text) {
  ScaledLines l;
  l.text = std::move(text);
  l.sizeKey = kShowValueSize;
  l.fallback = Show::kValueSize;
  return scaledLines(std::move(l));
}

template <class... As>
Block heroLines(std::string text, As&&... as) {
  ScaledLines l;
  l.text = std::move(text);
  l.sizeKey = kShowHeroSize;
  l.fallback = Show::kHeroSize;
  return scaledLines(std::move(l), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
