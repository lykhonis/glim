#pragma once

#include <string>
#include <utility>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Event.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/TextEdit.h"
#include "glim/toolkit/Tokens.h"
#include "glim/toolkit/theme/Keys.h"

namespace glim {
namespace toolkit {

// Where a field's text and caret sit inside its box; paint, caret and the
// IME's candidate anchor all read this one geometry.
struct FieldGeometry {
  static constexpr float kPadX = 8.0f;
  static constexpr float kPadY = 6.0f;
  static constexpr float kCaretWidth = 2.0f;
  static float height() { return Tokens::kTypeLine + 2 * kPadY; }
  // X of the byte offset within the field's text.
  static float offsetX(const Box& box, const std::string& text, std::size_t at, const Engine& e);
  static Box caret(const Box& box, const TextEdit& edit, const TextRules& rules, const Engine& e);
  // The cluster boundary in the text nearest to x.
  static std::size_t offsetAt(
    const Box& box, const std::string& text, const TextRules& rules, float x, const Engine& e
  );
  // The selection's highlight; empty for a caret.
  static Box highlight(
    const Box& box, const TextEdit& edit, const TextRules& rules, const Engine& e
  );
  static Box underline(
    const Box& box, const TextEdit& edit, const TextRules& rules, const Engine& e
  );
};

// Owned text input (N8): Glim draws the text and the shell's text input
// client feeds it TextChanges while it holds focus.
struct TextField : WithSemantic, WithA11y {
  static constexpr const char* kKind = "TextField";
  const char* kind() const { return kKind; }
  State<TextEdit> value;
  float width = 240.0f;
  std::string placeholder;
  TextRules rules;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(width, FieldGeometry::height(), c);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const;
  bool handleAt(const Box& box, const Event& ev) const;
  A11yTraits a11yTraits() const {
    return {.role = A11yRole::TextField, .value = shownText(value->text, rules)};
  }
};

template <class... As>
auto textField(TextField f, As&&... as) {
  configureTags(f, std::forward<As>(as)...);
  return finish(std::move(f), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
