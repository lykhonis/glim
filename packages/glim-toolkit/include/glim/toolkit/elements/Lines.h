#pragma once

#include <string>
#include <utility>
#include <vector>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Theme.h"
#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {

struct PlacedLine {
  Box box;
  std::string text;
};

inline std::vector<PlacedLine> layoutPlacedLines(
    const std::string& text, const Box& box, bool wrap, int align
) {
  std::vector<std::string> lines = detail::layoutLines(text, box.w, box.h, wrap);
  std::vector<PlacedLine> out;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    float lw = static_cast<float>(lines[i].size()) * Tokens::kTypeAdvance;
    float x = box.x;
    if (align == kAlignCenter) x += (box.w - lw) * 0.5f;
    if (align == kAlignRight) x += box.w - lw;
    out.push_back(
        {{x, box.y + static_cast<float>(i) * Tokens::kTypeLine, lw, Tokens::kTypeLine}, lines[i]}
    );
  }
  return out;
}

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
    for (const auto& l : layoutPlacedLines(text, box, wrap, align))
      rec.push(kKind, l.box, l.text, semantic);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const Theme& t = theme(e);
    for (const auto& l : layoutPlacedLines(text, box, wrap, align))
      sink.text(l.box.x, l.box.y, l.text, t.inkOnLight, t.typeLine, semantic);
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
