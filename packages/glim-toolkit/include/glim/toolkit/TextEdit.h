#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace glim {
namespace toolkit {

// Byte offsets into UTF-8 text. Shells convert from their platform's units.
struct TextRange {
  std::size_t start = 0;
  std::size_t length = 0;
  std::size_t end() const { return start + length; }
};

// A state the field can return to: undo and redo restore these.
struct TextSnapshot {
  std::string text;
  TextRange selection;
  bool reversed = false;
};

// What a field accepts. Limits count grapheme clusters; accept filters code
// points (rejected ones are dropped). Secure fields show one bullet per
// cluster and never give their text away.
struct TextRules {
  std::size_t maxLength = 0;  // 0 = unlimited
  bool (*accept)(char32_t) = nullptr;
  bool secure = false;
};

// How far a move or delete reaches. Lines are hard lines until layout
// supplies wrapped ones.
enum class TextUnit : std::uint8_t { Character, Word, Line, Document };

enum class TextOp : std::uint8_t {
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
  Select,
};

// Undo and redo stacks. Typing and repeated deletes coalesce into one
// step until another kind of command runs.
struct TextHistory {
  std::vector<TextSnapshot> undo;
  std::vector<TextSnapshot> redo;
  std::optional<TextOp> streak;
};

// The last press on a field, so the next one can count as a double or
// triple tap.
struct TextPress {
  float time = -1.f;
  float x = 0.f;
  float y = 0.f;
  int taps = 0;
};

// An owned field's editing state: text, selection (a caret when empty),
// and the IME's marked run while a composition is open. reversed puts the
// moving end of the selection at its start, so Shift moves extend it there.
struct TextEdit {
  std::string text;
  TextRange selection;
  std::optional<TextRange> marked;
  bool reversed = false;
  TextHistory history;
  TextPress press;
};

// One command from a shell's text input client. Mark carries its selection
// relative to the marked run; moves and deletes reach one unit, and extend
// grows the selection instead of moving the caret.
struct TextChange {
  TextOp op = TextOp::Insert;
  std::string text;
  TextRange selection;
  TextUnit unit = TextUnit::Character;
  bool extend = false;
};

// Commits text over the marked run if any, else over the selection.
TextEdit insertText(TextEdit edit, std::string_view text, const TextRules& rules = {});
// Opens or replaces the composition; empty text cancels it.
TextEdit markText(TextEdit edit, std::string_view text, TextRange selection);
// Commits the marked run as typed.
TextEdit unmarkText(TextEdit edit);
// Deletes the selection, else one unit before (direction < 0) or after the
// caret.
TextEdit deleteText(TextEdit edit, int direction, TextUnit unit = TextUnit::Character);
TextEdit deleteBackward(TextEdit edit);
// Collapses a selection to its side, else steps one unit; extend moves only
// the selection's moving end.
TextEdit moveCaret(
  TextEdit edit, int direction, TextUnit unit = TextUnit::Character, bool extend = false
);
// Selects a range the platform chose, snapped out to whole clusters; any
// composition commits first.
TextEdit selectText(TextEdit edit, TextRange range);
// Selects the word (or run of spaces or symbols) at the caret.
TextEdit selectWord(TextEdit edit);
TextEdit selectAll(TextEdit edit);
// A press at byte offset at: one tap places the caret (or extends with
// shift), two select the word, three select all. Taps count when they land
// close together in time and space.
TextEdit pressText(TextEdit edit, std::size_t at, float x, float y, float time, bool shift = false);
// A drag to at moves the selection's moving end.
TextEdit dragText(TextEdit edit, std::size_t at);
TextEdit undo(TextEdit edit);
TextEdit redo(TextEdit edit);
TextEdit applyText(TextEdit edit, const TextChange& change, const TextRules& rules = {});

// What a copy takes: the selection, or nothing for a secure field or an
// empty selection.
std::optional<std::string> copyText(const TextEdit& edit, const TextRules& rules = {});
// Edit menu actions, in menu order.
enum class TextAction : std::uint8_t { Cut, Copy, Paste, SelectAll };
// The actions that apply now: cut and copy need copyText, paste needs
// clipboard text, select all needs text not yet all selected.
std::vector<TextAction> textActions(const TextEdit& edit, const TextRules& rules, bool canPaste);
// What an action does: text for the clipboard, then a change to the field.
struct TextActionStep {
  std::optional<std::string> copy;
  std::optional<TextChange> change;
};
// pasted is the clipboard's text, read only for Paste.
TextActionStep textActionStep(
  TextAction action, const TextEdit& edit, const TextRules& rules, std::string_view pasted
);
// What the field shows: the text, or one bullet per cluster when secure.
std::string shownText(const std::string& text, const TextRules& rules);
// A byte offset into the text, mapped into shownText.
std::size_t shownOffset(const std::string& text, std::size_t at, const TextRules& rules);

}  // namespace toolkit
}  // namespace glim
