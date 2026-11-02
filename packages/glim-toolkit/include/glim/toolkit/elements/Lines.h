#pragma once

#include <string>
#include <utility>
#include <vector>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {

struct Lines : WithAlign, WithSemantic, WithWrap {
  static constexpr const char* kKind = "Lines";
  const char* kind() const { return kKind; }
  std::string text;
  Box place(Constraints c, const Engine& e) const {
    if (!wrap) return detail::measureText(text, c, e);
    // Wrapped natural size: full wrap at maxW (unbounded → single line),
    // no ellipsis on the layout pass (overflow spills; Roll re-flows).
    std::vector<std::string> lines = detail::wrapText(text, c.maxW);
    float w = 0;
    for (const auto& l : lines) {
      float lw = static_cast<float>(l.size()) * Tokens::kTypeAdvance;
      if (lw > w) w = lw;
    }
    return detail::resolveSize(w, static_cast<float>(lines.size()) * Tokens::kTypeLine, c);
  }
  void record(const Box& box, Rec& rec) const {
    std::vector<std::string> lines = detail::layoutLines(text, box.w, box.h, wrap);
    for (std::size_t i = 0; i < lines.size(); ++i) {
      float lw = static_cast<float>(lines[i].size()) * Tokens::kTypeAdvance;
      float x = box.x;
      if (align == kAlignCenter) x += (box.w - lw) * 0.5f;
      if (align == kAlignRight) x += box.w - lw;
      Box b{x, box.y + static_cast<float>(i) * Tokens::kTypeLine, lw, Tokens::kTypeLine};
      rec.push(kKind, b, lines[i], semantic);
    }
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
