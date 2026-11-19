#pragma once

#include <glim/math.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace glim::shell {

// Byte offsets into UTF-8 text; each shell converts to its platform's units.
struct TextInputRange {
  std::size_t start = 0;
  std::size_t length = 0;
};

enum class TextInputOp : std::uint8_t {
  Insert,
  Mark,
  Unmark,
  DeleteBackward,
  MoveLeft,
  MoveRight,
  SelectAll,
  DeleteForward,
  SelectWord,
  Undo,
  Redo,
  // The platform set the selection (UIKit, Android, the browser).
  Select,
};

// How far a move or delete reaches.
enum class TextInputUnit : std::uint8_t { Character, Word, Line, Document };

// What the platform's text input client asks of the focused field. Mark
// carries its selection relative to the marked text; extend grows the
// selection instead of moving the caret.
struct TextInputChange {
  TextInputOp op = TextInputOp::Insert;
  std::string text;
  TextInputRange selection;
  TextInputUnit unit = TextInputUnit::Character;
  bool extend = false;
};

// Where a boundary in the text paints: a byte offset and its window x.
struct TextInputStop {
  std::size_t at = 0;
  float x = 0;
};

// The focused field as the platform's text input client sees it. Inactive
// when focus edits no text; caret is window-logical, the candidate anchor.
// A secure field shows only its mask and never copies.
struct TextInputState {
  bool active = false;
  bool secure = false;
  std::string text;
  TextInputRange selection;
  std::optional<TextInputRange> marked;
  Rect caret{};
  // Every cluster boundary, first to last, on the caret's line; platforms
  // place selection handles and map points to positions with them.
  std::vector<TextInputStop> stops;
};

inline bool stopsEqual(const std::vector<TextInputStop>& a, const std::vector<TextInputStop>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].at != b[i].at || a[i].x != b[i].x) return false;
  }
  return true;
}

// Whether a shell needs to tell its platform anything new.
inline bool sameTextInput(const TextInputState& a, const TextInputState& b) {
  const auto range = [](const TextInputRange& x, const TextInputRange& y) {
    return x.start == y.start && x.length == y.length;
  };
  const bool marked =
    a.marked.has_value() == b.marked.has_value() && (!a.marked || range(*a.marked, *b.marked));
  return a.active == b.active && a.secure == b.secure && a.text == b.text &&
         range(a.selection, b.selection) && marked && a.caret.origin.x == b.caret.origin.x &&
         a.caret.origin.y == b.caret.origin.y && a.caret.size.x == b.caret.size.x &&
         a.caret.size.y == b.caret.size.y && stopsEqual(a.stops, b.stops);
}

using TextInputCallback = std::function<void(const TextInputChange&)>;

}  // namespace glim::shell
