#include "glim/toolkit/TextEdit.h"

#include <algorithm>
#include <cmath>

#include "text/Segment.h"

namespace glim {
namespace toolkit {

namespace {

constexpr std::size_t kUndoDepth = 100;
constexpr std::string_view kBullet = "•";
constexpr float kTapSeconds = 0.4f;
constexpr float kTapSlop = 6.f;

// Shell ranges are untrusted: clamp into the text and onto code points.
TextRange clamp(const std::string& s, TextRange r) {
  const std::size_t a = text::snapPoint(s, std::min(r.start, s.size()));
  std::size_t b = std::min(r.start + std::min(r.length, s.size()), s.size());
  if (text::snapPoint(s, b) != b) text::decodeAt(s, text::snapPoint(s, b), &b);
  return {a, std::max(a, b) - a};
}

TextSnapshot snapshot(const TextEdit& e) { return {e.text, e.selection, e.reversed}; }

void restore(TextEdit& e, TextSnapshot s) {
  e.text = std::move(s.text);
  e.selection = s.selection;
  e.reversed = s.reversed;
  e.marked.reset();
  e.history.streak.reset();
}

// Saves the state before an edit, unless it continues a streak of the
// same coalescing command; any edit drops the redo stack.
void record(TextEdit& e, std::optional<TextOp> streak) {
  TextHistory& h = e.history;
  if (!streak || h.streak != streak) {
    h.undo.push_back(snapshot(e));
    if (h.undo.size() > kUndoDepth) h.undo.erase(h.undo.begin());
  }
  h.redo.clear();
  h.streak = streak;
}

TextEdit replace(TextEdit e, TextRange r, std::string_view text) {
  r = clamp(e.text, r);
  e.text.replace(r.start, r.length, text);
  e.selection = {r.start + text.size(), 0};
  e.reversed = false;
  e.marked.reset();
  return e;
}

TextRange target(const TextEdit& e) { return e.marked ? *e.marked : e.selection; }

// Where one unit from at reaches in a direction.
std::size_t reach(const std::string& s, std::size_t at, int direction, TextUnit unit) {
  const bool back = direction < 0;
  switch (unit) {
    case TextUnit::Character:
      return back ? text::prevCluster(s, at) : text::nextCluster(s, at);
    case TextUnit::Word:
      return back ? text::prevWordStart(s, at) : text::nextWordEnd(s, at);
    case TextUnit::Line:
      return back ? text::lineStart(s, at) : text::lineEnd(s, at);
    case TextUnit::Document:
      return back ? 0 : s.size();
  }
  return at;
}

// The text with rejected code points dropped, cut to at most room clusters.
std::string admit(std::string_view in, const TextRules& rules, std::size_t room) {
  std::string out;
  for (std::size_t i = 0; i < in.size();) {
    std::size_t next = in.size();
    const char32_t c = text::decodeAt(in, i, &next);
    if (!rules.accept || rules.accept(c)) out.append(in.substr(i, next - i));
    i = next;
  }
  if (rules.maxLength == 0) return out;
  std::size_t end = 0;
  for (std::size_t n = 0; n < room && end < out.size(); ++n) end = text::nextCluster(out, end);
  out.resize(std::min(end, out.size()));
  return out;
}

}  // namespace

TextEdit insertText(TextEdit edit, std::string_view text, const TextRules& rules) {
  const TextRange r = clamp(edit.text, target(edit));
  std::size_t room = 0;
  if (rules.maxLength > 0) {
    const std::size_t kept =
      text::clusterCount(edit.text) -
      text::clusterCount(std::string_view(edit.text).substr(r.start, r.length));
    room = rules.maxLength > kept ? rules.maxLength - kept : 0;
  }
  const std::string admitted = admit(text, rules, room);
  if (admitted.empty() && r.length == 0 && !edit.marked) return edit;
  if (!edit.marked) record(edit, TextOp::Insert);
  return replace(std::move(edit), r, admitted);
}

TextEdit markText(TextEdit edit, std::string_view text, TextRange selection) {
  const TextRange r = clamp(edit.text, target(edit));
  if (!edit.marked && !text.empty()) record(edit, TextOp::Insert);
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

TextEdit deleteText(TextEdit edit, int direction, TextUnit unit) {
  TextRange r = clamp(edit.text, target(edit));
  if (r.length == 0) {
    std::size_t to = reach(edit.text, r.start, direction, unit);
    if (to == r.start) to = reach(edit.text, r.start, direction, TextUnit::Character);
    r = {std::min(to, r.start), std::max(to, r.start) - std::min(to, r.start)};
  }
  if (r.length == 0) return edit;
  const TextOp op = direction < 0 ? TextOp::DeleteBackward : TextOp::DeleteForward;
  record(edit, unit == TextUnit::Character ? std::optional<TextOp>(op) : std::nullopt);
  return replace(std::move(edit), r, {});
}

TextEdit deleteBackward(TextEdit edit) { return deleteText(std::move(edit), -1); }

TextEdit moveCaret(TextEdit edit, int direction, TextUnit unit, bool extend) {
  edit = unmarkText(std::move(edit));
  edit.history.streak.reset();
  const TextRange r = clamp(edit.text, edit.selection);
  if (extend) {
    const std::size_t anchor = edit.reversed ? r.end() : r.start;
    const std::size_t moved = reach(edit.text, edit.reversed ? r.start : r.end(), direction, unit);
    edit.selection = {std::min(anchor, moved), std::max(anchor, moved) - std::min(anchor, moved)};
    edit.reversed = moved < anchor;
    return edit;
  }
  const std::size_t side = direction < 0 ? r.start : r.end();
  const bool collapse = r.length > 0 && unit == TextUnit::Character;
  edit.selection = {collapse ? side : reach(edit.text, side, direction, unit), 0};
  edit.reversed = false;
  return edit;
}

TextEdit selectText(TextEdit edit, TextRange range) {
  edit = unmarkText(std::move(edit));
  edit.history.streak.reset();
  const std::string& s = edit.text;
  // A boundary inside a cluster moves to the cluster's start or end.
  const auto down = [&](std::size_t at) {
    return at >= s.size() ? s.size() : text::prevCluster(s, text::nextCluster(s, at));
  };
  const auto up = [&](std::size_t at) { return down(at) == at ? at : text::nextCluster(s, at); };
  const TextRange r = clamp(s, range);
  const std::size_t a = down(r.start);
  const std::size_t b = r.length == 0 ? a : up(r.end());
  edit.selection = {a, b - a};
  edit.reversed = false;
  return edit;
}

TextEdit selectWord(TextEdit edit) {
  edit = unmarkText(std::move(edit));
  edit.history.streak.reset();
  const text::Span span = text::runAt(edit.text, clamp(edit.text, edit.selection).start);
  edit.selection = {span.start, span.end - span.start};
  edit.reversed = false;
  return edit;
}

TextEdit selectAll(TextEdit edit) {
  edit.marked.reset();
  edit.history.streak.reset();
  edit.selection = {0, edit.text.size()};
  edit.reversed = false;
  return edit;
}

TextEdit pressText(TextEdit edit, std::size_t at, float x, float y, float time, bool shift) {
  edit = unmarkText(std::move(edit));
  edit.history.streak.reset();
  at = clamp(edit.text, {at, 0}).start;
  const TextPress& last = edit.press;
  const bool repeat = !shift && last.time >= 0.f && time - last.time <= kTapSeconds &&
                      std::abs(x - last.x) <= kTapSlop && std::abs(y - last.y) <= kTapSlop;
  edit.press = {time, x, y, repeat ? last.taps + 1 : 1};
  if (edit.press.taps >= 3) return selectAll(std::move(edit));
  if (edit.press.taps == 2) {
    edit.selection = {at, 0};
    return selectWord(std::move(edit));
  }
  if (shift) return dragText(std::move(edit), at);
  edit.selection = {at, 0};
  edit.reversed = false;
  return edit;
}

TextEdit dragText(TextEdit edit, std::size_t at) {
  const TextRange r = clamp(edit.text, edit.selection);
  at = clamp(edit.text, {at, 0}).start;
  const std::size_t anchor = edit.reversed ? r.end() : r.start;
  const std::size_t moved = at;
  edit.selection = {std::min(anchor, moved), std::max(anchor, moved) - std::min(anchor, moved)};
  edit.reversed = moved < anchor;
  return edit;
}

TextEdit undo(TextEdit edit) {
  TextHistory& h = edit.history;
  if (h.undo.empty()) return edit;
  TextSnapshot back = std::move(h.undo.back());
  h.undo.pop_back();
  h.redo.push_back(snapshot(edit));
  restore(edit, std::move(back));
  return edit;
}

TextEdit redo(TextEdit edit) {
  TextHistory& h = edit.history;
  if (h.redo.empty()) return edit;
  TextSnapshot ahead = std::move(h.redo.back());
  h.redo.pop_back();
  h.undo.push_back(snapshot(edit));
  restore(edit, std::move(ahead));
  return edit;
}

TextEdit applyText(TextEdit edit, const TextChange& change, const TextRules& rules) {
  switch (change.op) {
    case TextOp::Insert:
      return insertText(std::move(edit), change.text, rules);
    case TextOp::Mark:
      return markText(std::move(edit), change.text, change.selection);
    case TextOp::Unmark:
      return unmarkText(std::move(edit));
    case TextOp::DeleteBackward:
      return deleteText(std::move(edit), -1, change.unit);
    case TextOp::DeleteForward:
      return deleteText(std::move(edit), 1, change.unit);
    case TextOp::MoveLeft:
      return moveCaret(std::move(edit), -1, change.unit, change.extend);
    case TextOp::MoveRight:
      return moveCaret(std::move(edit), 1, change.unit, change.extend);
    case TextOp::SelectAll:
      return selectAll(std::move(edit));
    case TextOp::Select:
      return selectText(std::move(edit), change.selection);
    case TextOp::SelectWord:
      return selectWord(std::move(edit));
    case TextOp::Undo:
      return undo(std::move(edit));
    case TextOp::Redo:
      return redo(std::move(edit));
  }
  return edit;
}

std::optional<std::string> copyText(const TextEdit& edit, const TextRules& rules) {
  const TextRange r = clamp(edit.text, edit.selection);
  if (rules.secure || r.length == 0) return std::nullopt;
  return edit.text.substr(r.start, r.length);
}

std::vector<TextAction> textActions(const TextEdit& edit, const TextRules& rules, bool canPaste) {
  std::vector<TextAction> out;
  if (copyText(edit, rules)) out.insert(out.end(), {TextAction::Cut, TextAction::Copy});
  if (canPaste) out.push_back(TextAction::Paste);
  if (clamp(edit.text, edit.selection).length < edit.text.size()) {
    out.push_back(TextAction::SelectAll);
  }
  return out;
}

TextActionStep textActionStep(
  TextAction action, const TextEdit& edit, const TextRules& rules, std::string_view pasted
) {
  switch (action) {
    case TextAction::Cut: {
      std::optional<std::string> copy = copyText(edit, rules);
      if (!copy) return {};
      return {std::move(copy), TextChange{.op = TextOp::DeleteBackward}};
    }
    case TextAction::Copy:
      return {copyText(edit, rules), std::nullopt};
    case TextAction::Paste:
      return {std::nullopt, TextChange{.op = TextOp::Insert, .text = std::string(pasted)}};
    case TextAction::SelectAll:
      return {std::nullopt, TextChange{.op = TextOp::SelectAll}};
  }
  return {};
}

std::string shownText(const std::string& text, const TextRules& rules) {
  if (!rules.secure) return text;
  std::string out;
  for (std::size_t n = text::clusterCount(text); n > 0; --n) out.append(kBullet);
  return out;
}

std::size_t shownOffset(const std::string& text, std::size_t at, const TextRules& rules) {
  at = std::min(at, text.size());
  if (!rules.secure) return at;
  return text::clusterCount(std::string_view(text).substr(0, at)) * kBullet.size();
}

}  // namespace toolkit
}  // namespace glim
