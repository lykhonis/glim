#include "glim/toolkit/TextEdit.h"

#include <algorithm>

namespace glim {
namespace toolkit {

namespace {

bool continuation(char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }

std::size_t prevPoint(const std::string& s, std::size_t i) {
  if (i == 0) return 0;
  --i;
  while (i > 0 && continuation(s[i])) --i;
  return i;
}

std::size_t nextPoint(const std::string& s, std::size_t i) {
  if (i >= s.size()) return s.size();
  ++i;
  while (i < s.size() && continuation(s[i])) ++i;
  return i;
}

// Shell ranges are untrusted: clamp into the text and onto code points.
TextRange clamp(const std::string& s, TextRange r) {
  std::size_t a = std::min(r.start, s.size());
  std::size_t b = std::min(r.start + std::min(r.length, s.size()), s.size());
  while (a > 0 && a < s.size() && continuation(s[a])) --a;
  while (b < s.size() && continuation(s[b])) ++b;
  return {a, std::max(a, b) - a};
}

TextEdit replace(TextEdit e, TextRange r, std::string_view text) {
  r = clamp(e.text, r);
  e.text.replace(r.start, r.length, text);
  e.selection = {r.start + text.size(), 0};
  e.marked.reset();
  return e;
}

TextRange target(const TextEdit& e) { return e.marked ? *e.marked : e.selection; }

}  // namespace

TextEdit insertText(TextEdit edit, std::string_view text) {
  const TextRange r = target(edit);
  return replace(std::move(edit), r, text);
}

TextEdit markText(TextEdit edit, std::string_view text, TextRange selection) {
  const TextRange r = clamp(edit.text, target(edit));
  edit = replace(std::move(edit), r, text);
  if (text.empty()) return edit;
  const std::string marked(text);
  const TextRange inner = clamp(marked, selection);
  edit.marked = TextRange{r.start, text.size()};
  edit.selection = {r.start + inner.start, inner.length};
  return edit;
}

TextEdit unmarkText(TextEdit edit) {
  if (!edit.marked) return edit;
  edit.selection = {edit.marked->end(), 0};
  edit.marked.reset();
  return edit;
}

TextEdit deleteBackward(TextEdit edit) {
  TextRange r = clamp(edit.text, target(edit));
  if (r.length == 0) {
    const std::size_t from = prevPoint(edit.text, r.start);
    r = {from, r.start - from};
  }
  return replace(std::move(edit), r, {});
}

TextEdit moveCaret(TextEdit edit, int direction) {
  edit = unmarkText(std::move(edit));
  const TextRange r = clamp(edit.text, edit.selection);
  std::size_t at = direction < 0 ? r.start : r.end();
  if (r.length == 0) at = direction < 0 ? prevPoint(edit.text, at) : nextPoint(edit.text, at);
  edit.selection = {at, 0};
  return edit;
}

TextEdit applyText(TextEdit edit, const TextChange& change) {
  switch (change.op) {
    case TextOp::Insert:
      return insertText(std::move(edit), change.text);
    case TextOp::Mark:
      return markText(std::move(edit), change.text, change.selection);
    case TextOp::Unmark:
      return unmarkText(std::move(edit));
    case TextOp::DeleteBackward:
      return deleteBackward(std::move(edit));
    case TextOp::MoveLeft:
      return moveCaret(std::move(edit), -1);
    case TextOp::MoveRight:
      return moveCaret(std::move(edit), 1);
    case TextOp::SelectAll: {
      const std::size_t size = edit.text.size();
      return {.text = std::move(edit.text), .selection = {0, size}};
    }
  }
  return edit;
}

}  // namespace toolkit
}  // namespace glim
