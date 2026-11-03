#pragma once

#include <string>
#include <utility>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Measure.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Tokens.h"
#include "glim/toolkit/text/Text.h"
#include "glim/toolkit/theme/Keys.h"

namespace glim {
namespace toolkit {

// Display numerals (T10, example-local): stat values and other hero
// numbers at exactly 2x body. A fixed-size sink branch — no entry or
// codec change — rather than a size field on every entry.
struct Display : WithSemantic {
  static constexpr const char* kKind = "Display";
  std::string text;
  Box place(Constraints c, const Engine& e) const {
    float w = static_cast<float>(text.size()) * Tokens::kTypeAdvance * 2.0f;
    if (const Measurer* m = e.get<Measurer>()) {
      if (m->measure) w = m->measure(text.c_str(), Tokens::kTypeDisplay);
    }
    return detail::resolveSize(w, Tokens::kTypeDisplay, c);
  }
  void record(const Box& box, Rec& rec) const { rec.push(kKind, box, text); }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    sink.text(
        box.x,
        box.y + (box.h - themeFloat(e, kThemeTypeDisplay, Tokens::kTypeDisplay)) * 0.5f,
        text,
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight),
        themeFloat(e, kThemeTypeDisplay, Tokens::kTypeDisplay)
    );
  }
};

template <class... As>
Block display(std::string text, As&&... as) {
  Display d;
  d.text = std::move(text);
  configureTags(d, std::forward<As>(as)...);
  return finish(std::move(d), std::forward<As>(as)...);
}

// Vertical hairline divider for pill rows: 2px rule, 32 tall,
// centered by the parent row.
struct Divider : WithSemantic {
  static constexpr const char* kKind = "Hairline";
  Box place(Constraints c, const Engine&) const { return detail::resolveSize(2.0f, 32.0f, c); }
  void record(const Box& box, Rec& rec) const { rec.push(kKind, box); }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    sink.fill(box, themeColor(e, kThemeHairline, Tokens::kHairline));
  }
};

inline Block divider() {
  Divider d;
  return finish(std::move(d));
}

// Dim label (stat captions ORDERS/VISITORS): same metrics as Lines,
// secondary ink via the Lines.dim sink branch. The reference styles
// captions gray over white values; a tag would need core support,
// so this stays a tiny example-local peer.
struct DimLines {
  static constexpr const char* kKind = "Lines.dim";
  const char* kind() const { return kKind; }
  std::string text;
  Box place(Constraints c, const Engine& e) const { return text::measureText(text, c, e); }
  void record(const Box& box, Rec& rec) const { rec.push(kKind, box, text, {}); }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    sink.text(
        box.x,
        box.y,
        text,
        themeColor(e, kThemeInkSecondary, Tokens::kInkSecondary),
        themeFloat(e, kThemeTypeLine, Tokens::kTypeLine)
    );
  }
};

inline Block dimLines(std::string text) {
  DimLines d;
  d.text = std::move(text);
  return finish(std::move(d));
}

}  // namespace toolkit
}  // namespace glim
