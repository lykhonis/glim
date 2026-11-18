#pragma once

#include <glim/math.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

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
};

// What the platform's text input client asks of the focused field. Mark
// carries its selection relative to the marked text.
struct TextInputChange {
  TextInputOp op = TextInputOp::Insert;
  std::string text;
  TextInputRange selection;
};

// The focused field as the platform's text input client sees it. Inactive
// when focus edits no text; caret is window-logical, the candidate anchor.
struct TextInputState {
  bool active = false;
  std::string text;
  TextInputRange selection;
  std::optional<TextInputRange> marked;
  Rect caret{};
};

using TextInputCallback = std::function<void(const TextInputChange&)>;

}  // namespace glim::shell
