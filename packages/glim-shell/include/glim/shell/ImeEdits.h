#pragma once

#include <glim/shell/TextInput.h>
#include <glim/shell/Utf16.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace glim::shell {

// IME edits as Android's InputConnection and the browser phrase them, in
// UTF-16 units against the field's published state, mapped to changes.

// Opens or replaces the composition, caret at its end.
inline std::vector<TextInputChange> imeCompose(const TextInputState&, std::string text) {
  const std::size_t n = text.size();
  return {{.op = TextInputOp::Mark, .text = std::move(text), .selection = {n, 0}}};
}

// Commits text over the composition, else over the selection.
inline std::vector<TextInputChange> imeCommit(const TextInputState&, std::string text) {
  return {{.op = TextInputOp::Insert, .text = std::move(text)}};
}

inline std::vector<TextInputChange> imeSelect(
  const TextInputState& state, std::size_t start, std::size_t end
) {
  const std::size_t a = std::min(start, end);
  return {
    {.op = TextInputOp::Select, .selection = utf16Bytes(state.text, a, std::max(start, end) - a)}
  };
}

// Deletes before bytes ahead of the selection and after bytes past it;
// the selection itself stays.
inline std::vector<TextInputChange> imeDeleteSurroundingBytes(
  const TextInputState& state, std::size_t before, std::size_t after
) {
  const std::size_t start = state.selection.start;
  const std::size_t end = start + state.selection.length;
  const std::size_t from = start - std::min(before, start);
  const std::size_t to = std::min(state.text.size(), end + after);
  std::vector<TextInputChange> out;
  const auto erase = [&](std::size_t a, std::size_t b) {
    out.push_back({.op = TextInputOp::Select, .selection = {a, b - a}});
    out.push_back({.op = TextInputOp::Insert});
  };
  if (to > end) erase(end, to);
  if (from < start) erase(from, start);
  if (!out.empty()) {
    out.push_back({.op = TextInputOp::Select, .selection = {from, state.selection.length}});
  }
  return out;
}

// The same in UTF-16 units, as Android and the browser count.
inline std::vector<TextInputChange> imeDeleteSurrounding(
  const TextInputState& state, std::size_t before, std::size_t after
) {
  const std::string& text = state.text;
  const std::size_t start = state.selection.start;
  const std::size_t end = start + state.selection.length;
  const std::size_t startUnits = utf16Units(text, start);
  const std::size_t from = utf16Byte(text, startUnits - std::min(before, startUnits));
  const std::size_t to = utf16Byte(text, utf16Units(text, end) + after);
  return imeDeleteSurroundingBytes(state, start - from, to - end);
}

// One atomic IME update, as Wayland's text-input-v3 applies on done, in
// bytes: the old preedit goes, deleteBefore and deleteAfter bytes around
// the cursor go, commit goes in, then preedit with its cursor (negative
// puts the cursor at the preedit's end).
struct ImeUpdate {
  std::size_t deleteBefore = 0;
  std::size_t deleteAfter = 0;
  std::string commit;
  std::string preedit;
  int preeditCursor = -1;
};

inline std::vector<TextInputChange> imeUpdate(const TextInputState& state, ImeUpdate update) {
  std::vector<TextInputChange> out;
  TextInputState plain = state;
  if (state.marked) {
    out.push_back({.op = TextInputOp::Mark});
    plain.text.erase(state.marked->start, state.marked->length);
    plain.selection = {state.marked->start, 0};
    plain.marked.reset();
  }
  for (TextInputChange& c :
       imeDeleteSurroundingBytes(plain, update.deleteBefore, update.deleteAfter)) {
    out.push_back(std::move(c));
  }
  if (!update.commit.empty()) {
    out.push_back({.op = TextInputOp::Insert, .text = std::move(update.commit)});
  }
  if (!update.preedit.empty()) {
    const std::size_t n = update.preedit.size();
    const std::size_t at =
      update.preeditCursor < 0 ? n : std::min(n, static_cast<std::size_t>(update.preeditCursor));
    out.push_back({.op = TextInputOp::Mark, .text = std::move(update.preedit), .selection = {at, 0}}
    );
  }
  return out;
}

}  // namespace glim::shell
