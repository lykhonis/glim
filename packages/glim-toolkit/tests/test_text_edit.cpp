#include <cassert>
#include <string>

#include "glim/toolkit/TextEdit.h"
#include "test_helpers.h"

using namespace glim::toolkit;

namespace {

TextEdit at(std::string text, std::size_t caret) {
  return {.text = std::move(text), .selection = {caret, 0}};
}

}  // namespace

void test_text_edit() {
  // Select all spans the text and drops any composition.
  {
    TextEdit e = applyText(
      {.text = "hello", .selection = {2, 0}, .marked = TextRange{1, 1}}, {.op = TextOp::SelectAll}
    );
    assert(e.selection.start == 0 && e.selection.length == 5 && !e.marked);
  }

  // Insert replaces the selection and leaves a collapsed caret after it.
  {
    TextEdit e = insertText(at("hello", 5), "!");
    assert(e.text == "hello!" && e.selection.start == 6 && e.selection.length == 0);
    e = insertText({.text = "hello", .selection = {1, 3}}, "ipp");
    assert(e.text == "hippo" && e.selection.start == 4);
  }

  // Composition: marked text sits at the caret, is replaced in place, and
  // its own selection is relative to the marked run.
  {
    TextEdit e = markText(at("ab", 1), "n", {1, 0});
    assert(e.text == "anb" && e.marked && e.marked->start == 1 && e.marked->length == 1);
    assert(e.selection.start == 2);
    e = markText(e, "ni", {2, 0});
    assert(e.text == "anib" && e.marked->length == 2 && e.selection.start == 3);
    e = insertText(e, "你");
    assert(e.text == "a你b" && !e.marked && e.selection.start == 4);
  }

  // Unmark commits the marked run as typed; an empty mark cancels it.
  {
    TextEdit e = unmarkText(markText(at("", 0), "ka", {2, 0}));
    assert(e.text == "ka" && !e.marked && e.selection.start == 2);
    e = markText(markText(at("x", 1), "k", {1, 0}), "", {0, 0});
    assert(e.text == "x" && !e.marked && e.selection.start == 1);
  }

  // Delete and caret moves step whole UTF-8 code points.
  {
    TextEdit e = deleteBackward(at("a你", 4));
    assert(e.text == "a" && e.selection.start == 1);
    e = deleteBackward({.text = "hello", .selection = {1, 3}});
    assert(e.text == "ho" && e.selection.start == 1);
    assert(deleteBackward(at("x", 0)).text == "x");
    e = moveCaret(at("a你b", 4), -1);
    assert(e.selection.start == 1);
    e = moveCaret(e, 1);
    assert(e.selection.start == 4);
    assert(moveCaret(at("ab", 2), 1).selection.start == 2);
    assert(moveCaret({.text = "abc", .selection = {1, 1}}, -1).selection.start == 1);
  }

  // One entry point applies a shell's change.
  {
    TextEdit e = applyText(at("", 0), {.op = TextOp::Mark, .text = "n", .selection = {1, 0}});
    e = applyText(e, {.op = TextOp::Insert, .text = "ñ"});
    e = applyText(e, {.op = TextOp::DeleteBackward});
    e = applyText(e, {.op = TextOp::Insert, .text = "ok"});
    assert(e.text == "ok" && e.selection.start == 2);
  }

  // Out-of-range input from a shell clamps instead of corrupting.
  {
    TextEdit e = insertText({.text = "ab", .selection = {9, 9}}, "c");
    assert(e.text == "abc" && e.selection.start == 3);
  }
}

void test_text_actions() {
  using A = TextAction;
  const auto actions = [](TextEdit e, bool canPaste, TextRules r = {}) {
    return textActions(e, r, canPaste);
  };
  // Empty field: only paste, and only with something to paste.
  assert(actions({}, false).empty());
  assert((actions({}, true) == std::vector<A>{A::Paste}));
  // A caret in text: paste and select all.
  assert((actions(at("ab", 1), true) == std::vector<A>{A::Paste, A::SelectAll}));
  // A selection: cut and copy; select all until everything is selected.
  const TextEdit part{.text = "abc", .selection = {0, 2}};
  assert((actions(part, false) == std::vector<A>{A::Cut, A::Copy, A::SelectAll}));
  const TextEdit all{.text = "abc", .selection = {0, 3}};
  assert((actions(all, true) == std::vector<A>{A::Cut, A::Copy, A::Paste}));
  // Secure fields never give their text away.
  assert((actions(part, true, {.secure = true}) == std::vector<A>{A::Paste, A::SelectAll}));
}

void test_text_field() {
  Store store;
  Engine e;
  e.store = store;
  State<TextEdit> value = rootScope(e).state("name", TextEdit{});
  const auto build = [&] {
    return column(lines("Name"), textField({.value = value, .width = 200}));
  };
  Block tree = build();
  Box tb = e.place(tree, {0, 720, 0, 480});
  Screen screen;

  // Nothing focused: no text target, and changes go nowhere.
  assert(!screen.textFocus(tree, tb, e));
  assert(!screen.text(tree, tb, {.op = TextOp::Insert, .text = "x"}, e));

  // A tap focuses the field; composition then commits through Screen.
  const Box field = focusBox(tree, tb, 0, e);
  assert(field.w == 200 && field.h > Tokens::kTypeLine);
  assert(screen.pointerDown(tree, tb, field.x + 4, field.y + 4, 1, e));
  assert(screen.text(tree, tb, {.op = TextOp::Mark, .text = "ni", .selection = {2, 0}}, e));
  assert(value->text == "ni" && value->marked);
  tree = build();
  std::optional<TextFocus> focus = screen.textFocus(tree, tb, e);
  assert(focus && focus->edit.marked && focus->field.y == field.y);
  assert(focus->caret.x > field.x && focus->caret.h > 0);
  assert(screen.text(tree, tb, {.op = TextOp::Insert, .text = "你"}, e));
  assert(value->text == "你" && !value->marked);

  // Paint: text, a caret, and an underline only while composing.
  const auto ops = [&] {
    tree = build();
    OpsRecorder rec;
    TestFeed feed;
    paintTree(tree, tb, e, rec, feed);
    int texts = 0, fills = 0;
    for (const auto& o : rec.ops) {
      texts += o.kind == Op::Kind::Text && o.text == value->text;
      fills += o.kind == Op::Kind::Fill;
    }
    return std::pair{texts, fills};
  };
  const auto [texts, plain] = ops();
  assert(texts == 1);
  screen.text(tree, tb, {.op = TextOp::Mark, .text = "a", .selection = {1, 0}}, e);
  assert(ops().second == plain + 1);

  // The caret paints at the focused field only, where textFocus puts it.
  {
    tree = build();
    OpsRecorder rec;
    paintCaret(tree, tb, screen.focus, rec, e);
    const Box caret = screen.textFocus(tree, tb, e)->caret;
    assert(rec.ops.size() == 1 && rec.ops[0].kind == Op::Kind::Fill);
    assert(rec.ops[0].box.x == caret.x && rec.ops[0].box.w == caret.w);
    OpsRecorder none;
    paintCaret(tree, tb, -1, none, e);
    assert(none.ops.empty());
  }

  // The caret holds steady after any press, key or text change, then
  // blinks in half-second phases; reduce motion keeps it on.
  {
    screen.clock.now = 5.f;
    screen.text(tree, tb, {.op = TextOp::MoveRight}, e);
    assert(screen.caretSince == 5.f);
    screen.clock.now = 5.4f;
    assert(screen.caretOn(false));
    screen.clock.now = 5.7f;
    assert(!screen.caretOn(false) && screen.caretOn(true));
    screen.clock.now = 6.2f;
    assert(screen.caretOn(false));
    screen.clock.now = 6.7f;
    assert(!screen.caretOn(false));
    screen.key(tree, tb, Key::Right, e);
    assert(screen.caretOn(false));
    screen.clock.now = 7.3f;
    screen.pointerDown(tree, tb, field.x + 4, field.y + 4, 1, e);
    screen.pointerUp(tree, tb, field.x + 4, field.y + 4, e);
    assert(screen.caretSince == 7.3f);
    tree = build();
  }

  // A11y reports a text field with its value.
  tree = build();
  const auto nodes = a11yNodes(tree, tb, e);
  assert(nodes.back().traits.role == A11yRole::TextField && nodes.back().traits.value == "你a");

  // Pointer: a tap places the caret at the nearest boundary, a drag
  // selects, and the selection paints a highlight under the text.
  {
    value = TextEdit{.text = "hello world"};
    tree = build();
    const float x0 = field.x + FieldGeometry::kPadX;
    const float adv = Tokens::kTypeAdvance;
    const float y = field.y + 4;
    screen.pointerUp(tree, tb, x0, y, e);
    screen.clock.now = 10.f;
    assert(screen.pointerDown(tree, tb, x0 + 3.4f * adv, y, 1, e));
    assert(value->selection.start == 3 && value->selection.length == 0);
    tree = build();
    screen.pointerMove(tree, tb, x0 + 8.6f * adv, y, true, 1, e);
    assert(value->selection.start == 3 && value->selection.length == 6);
    screen.pointerUp(tree, tb, x0 + 8.6f * adv, y, e);
    tree = build();
    OpsRecorder rec;
    TestFeed feed;
    paintTree(tree, tb, e, rec, feed);
    int lit = -1, txt = -1;
    for (std::size_t i = 0; i < rec.ops.size(); ++i) {
      const auto& o = rec.ops[i];
      if (o.kind == Op::Kind::Fill && o.box.x == x0 + 3 * adv && o.box.w == 6 * adv) lit = int(i);
      if (o.kind == Op::Kind::Text && o.text == "hello world") txt = int(i);
    }
    assert(lit >= 0 && lit < txt);
    // A double tap selects the word; Shift-click extends from the anchor.
    screen.clock.now = 20.f;
    screen.pointerDown(tree, tb, x0 + 1.2f * adv, y, 1, e);
    screen.pointerUp(tree, tb, x0 + 1.2f * adv, y, e);
    tree = build();
    screen.clock.now = 20.2f;
    screen.pointerDown(tree, tb, x0 + 1.2f * adv, y, 1, e);
    assert(value->selection.start == 0 && value->selection.length == 5);
    screen.pointerUp(tree, tb, x0, y, e);
    tree = build();
    screen.clock.now = 30.f;
    screen.pointerDown(tree, tb, x0 + 2 * adv, y, 1, e);
    screen.pointerUp(tree, tb, x0, y, e);
    tree = build();
    screen.pointerDown(tree, tb, x0 + 10 * adv, y, 1, e, true);
    assert(value->selection.start == 2 && value->selection.length == 8);
  }

  // A value wider than the field scrolls so the caret stays inside it;
  // paint, the caret and hit testing share that scroll, and paint clips.
  {
    const float adv = Tokens::kTypeAdvance;
    const std::string long_(40, 'x');
    const float inner = field.w - 2 * FieldGeometry::kPadX - FieldGeometry::kCaretWidth;
    value = TextEdit{.text = long_, .selection = {40, 0}};
    tree = build();
    const Box caret = screen.textFocus(tree, tb, e)->caret;
    assert(caret.x + caret.w <= field.x + field.w - FieldGeometry::kPadX + 0.01f);
    const float shift = 40 * adv - inner;
    assert(FieldGeometry::scroll(field, value.get(), {}, e) == shift);
    OpsRecorder rec;
    TestFeed feed;
    paintTree(tree, tb, e, rec, feed);
    bool clipped = false, shifted = false;
    for (const auto& o : rec.ops) {
      clipped |= o.kind == Op::Kind::PushClip && o.box.x == field.x && o.box.w == field.w;
      shifted |= o.kind == Op::Kind::Text && o.text == long_ &&
                 o.box.x == field.x + FieldGeometry::kPadX - shift;
    }
    assert(clipped && shifted);
    // A tap at the field's right edge lands near the end, not mid-text.
    screen.clock.now = 40.f;
    screen.pointerDown(tree, tb, caret.x, field.y + 4, 1, e);
    screen.pointerUp(tree, tb, caret.x, field.y + 4, e);
    assert(value->selection.start == 40);
    // At the start nothing scrolls.
    value = TextEdit{.text = long_, .selection = {0, 0}};
    assert(FieldGeometry::scroll(field, value.get(), {}, e) == 0.f);
  }
}
