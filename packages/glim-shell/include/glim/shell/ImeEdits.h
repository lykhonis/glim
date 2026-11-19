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

// Deletes before units ahead of the selection and after units past it;
// the selection itself stays.
inline std::vector<TextInputChange> imeDeleteSurrounding(
  const TextInputState& state, std::size_t before, std::size_t after
) {
  const std::string& text = state.text;
  const std::size_t start = state.selection.start;
  const std::size_t end = start + state.selection.length;
  const std::size_t startUnits = utf16Units(text, start);
  const std::size_t from = utf16Byte(text, startUnits - std::min(before, startUnits));
  const std::size_t to = utf16Byte(text, utf16Units(text, end) + after);
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

}  // namespace glim::shell
