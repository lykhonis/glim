#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Tokens.h"
#include "glim/toolkit/text/Text.h"
#include "glim/toolkit/theme/Keys.h"

namespace glim {
namespace toolkit {

struct Lines : WithAlign, WithSemantic, WithWrap {
  static constexpr const char* kKind = "Lines";
  const char* kind() const { return kKind; }
  std::string text;
  Box place(Constraints c, const Engine& e) const {
    if (!wrap) return text::measureText(text, c, e);
    // Wrapped natural size: full wrap at maxW (unbounded → single line),
    // no ellipsis on the layout pass (overflow spills; Roll re-flows).
    std::vector<std::string> lines = text::wrapText(text, c.maxW, e);
    float w = 0;
    for (const auto& l : lines) {
      float lw = static_cast<float>(l.size()) * Tokens::kTypeAdvance;
      if (lw > w) w = lw;
    }
    return detail::resolveSize(w, static_cast<float>(lines.size()) * Tokens::kTypeLine, c);
  }
  void record(const Box& box, Rec& rec) const {
    for (const auto& l : text::layoutPlacedLines(text, box, wrap, align))
      rec.push(kKind, l.box, l.text);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const std::uint32_t ink = themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight);
    const float size = themeFloat(e, kThemeTypeLine, Tokens::kTypeLine);
    for (const auto& l : text::layoutPlacedLines(text, box, wrap, align, e))
      sink.text(l.box.x, l.box.y, l.text, ink, size);
  }
};

template <class... As>
auto lines(std::string text, As&&... as) {
  Lines l;
  l.text = std::move(text);
  configureTags(l, std::forward<As>(as)...);
  return finish(std::move(l), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
