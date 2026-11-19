#include <glim/toolkit/host/TextInput.h>

namespace glim::toolkit::host {

namespace {

TextOp toolkitOp(shell::TextInputOp op) {
  switch (op) {
    case shell::TextInputOp::Insert:
      return TextOp::Insert;
    case shell::TextInputOp::Mark:
      return TextOp::Mark;
    case shell::TextInputOp::Unmark:
      return TextOp::Unmark;
    case shell::TextInputOp::DeleteBackward:
      return TextOp::DeleteBackward;
    case shell::TextInputOp::MoveLeft:
      return TextOp::MoveLeft;
    case shell::TextInputOp::MoveRight:
      return TextOp::MoveRight;
    case shell::TextInputOp::SelectAll:
      return TextOp::SelectAll;
    case shell::TextInputOp::DeleteForward:
      return TextOp::DeleteForward;
    case shell::TextInputOp::SelectWord:
      return TextOp::SelectWord;
    case shell::TextInputOp::Undo:
      return TextOp::Undo;
    case shell::TextInputOp::Redo:
      return TextOp::Redo;
  }
  return TextOp::Insert;
}

TextUnit toolkitUnit(shell::TextInputUnit unit) {
  switch (unit) {
    case shell::TextInputUnit::Character:
      return TextUnit::Character;
    case shell::TextInputUnit::Word:
      return TextUnit::Word;
    case shell::TextInputUnit::Line:
      return TextUnit::Line;
    case shell::TextInputUnit::Document:
      return TextUnit::Document;
  }
  return TextUnit::Character;
}

}  // namespace

shell::TextInputState shellTextInput(const std::optional<TextFocus>& focus) {
  if (!focus) return {};
  const TextEdit& e = focus->edit;
  const TextRules& rules = focus->rules;
  const auto shown = [&](TextRange r) {
    const std::size_t a = shownOffset(e.text, r.start, rules);
    return shell::TextInputRange{a, shownOffset(e.text, r.end(), rules) - a};
  };
  shell::TextInputState s;
  s.active = true;
  s.secure = rules.secure;
  s.text = shownText(e.text, rules);
  s.selection = shown(e.selection);
  if (e.marked) s.marked = shown(*e.marked);
  const Box& c = focus->caret;
  s.caret = {{c.x, c.y}, {c.w, c.h}};
  return s;
}

TextChange toolkitText(const shell::TextInputChange& change) {
  return {
    .op = toolkitOp(change.op),
    .text = change.text,
    .selection = {change.selection.start, change.selection.length},
    .unit = toolkitUnit(change.unit),
    .extend = change.extend,
  };
}

}  // namespace glim::toolkit::host
