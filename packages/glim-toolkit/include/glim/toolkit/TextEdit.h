#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace glim {
namespace toolkit {

// Byte offsets into UTF-8 text. Shells convert from their platform's units.
struct TextRange {
  std::size_t start = 0;
  std::size_t length = 0;
  std::size_t end() const { return start + length; }
};

// An owned field's editing state: text, selection (a caret when empty),
// and the IME's marked run while a composition is open.
struct TextEdit {
  std::string text;
  TextRange selection;
  std::optional<TextRange> marked;
};

enum class TextOp : std::uint8_t {
  Insert,
  Mark,
  Unmark,
  DeleteBackward,
  MoveLeft,
  MoveRight,
  SelectAll
};

// One change from a shell's text input client. Mark carries its selection
// relative to the marked run.
struct TextChange {
  TextOp op = TextOp::Insert;
  std::string text;
  TextRange selection;
};

// Commits text over the marked run if any, else over the selection.
TextEdit insertText(TextEdit edit, std::string_view text);
// Opens or replaces the composition; empty text cancels it.
TextEdit markText(TextEdit edit, std::string_view text, TextRange selection);
// Commits the marked run as typed.
TextEdit unmarkText(TextEdit edit);
// Deletes the selection, else the code point before the caret.
TextEdit deleteBackward(TextEdit edit);
// Collapses a selection to its side, else steps one code point.
TextEdit moveCaret(TextEdit edit, int direction);
TextEdit applyText(TextEdit edit, const TextChange& change);

}  // namespace toolkit
}  // namespace glim
