#include <cassert>
#include <string>

#include "glim/toolkit/TextEdit.h"
#include "test_helpers.h"

using namespace glim::toolkit;

namespace {

TextEdit at(std::string text, std::size_t caret) {
  return {.text = std::move(text), .selection = {caret, 0}};
}

TextEdit run(TextEdit e, TextChange c, const TextRules& rules = {}) {
  return applyText(std::move(e), c, rules);
}

TextEdit type(TextEdit e, const char* text) { return run(std::move(e), {.text = text}); }

bool sel(const TextEdit& e, std::size_t start, std::size_t length) {
  return e.selection.start == start && e.selection.length == length;
}

const TextChange kLeft{.op = TextOp::MoveLeft};
const TextChange kRight{.op = TextOp::MoveRight};

TextChange by(TextChange c, TextUnit unit, bool extend = false) {
  c.unit = unit;
  c.extend = extend;
  return c;
}

bool digits(char32_t c) { return c >= '0' && c <= '9'; }

// Man, woman and girl joined by ZWJ: one cluster of 18 bytes.
const std::string kFamily = "\U0001F468‍\U0001F469‍\U0001F467";
const std::string kThumb = "\U0001F44D\U0001F3FD";  // with a skin tone
const std::string kFlags = "\U0001F1FA\U0001F1E6\U0001F1EF\U0001F1F5";
const std::string kAcute = "é";
const std::string kKeycap = "1️⃣";

}  // namespace

void test_text_commands() {
  // Moves and deletes step whole grapheme clusters.
  {
    const std::string s = "a" + kFamily + "b";
    TextEdit e = run(at(s, 0), kRight);
    assert(e.selection.start == 1);
    e = run(e, kRight);
    assert(e.selection.start == 19);
    assert(run(e, kLeft).selection.start == 1);
    assert(run(e, {.op = TextOp::DeleteBackward}).text == "ab");
    assert(run(at(kFlags, 0), kRight).selection.start == 8);
    assert(run(at(kFlags, 16), kLeft).selection.start == 8);
    assert(run(at(kFlags.substr(0, 12), 8), kRight).selection.start == 12);
    assert(run(at(kAcute + "x", 0), kRight).selection.start == 3);
    assert(run(at(kThumb, 0), kRight).selection.start == 8);
    assert(run(at(kKeycap, 0), kRight).selection.start == 7);
    assert(run(at("a\r\nb", 1), kRight).selection.start == 3);
    assert(run(at(kAcute, 3), {.op = TextOp::DeleteBackward}).text.empty());
  }

  // Forward delete removes the selection, else the cluster after the caret.
  {
    assert(run(at("abc", 1), {.op = TextOp::DeleteForward}).text == "ac");
    assert(run(at("abc", 3), {.op = TextOp::DeleteForward}).text == "abc");
    assert(run({.text = "abcd", .selection = {1, 2}}, {.op = TextOp::DeleteForward}).text == "ad");
    assert(run(at("x" + kThumb, 1), {.op = TextOp::DeleteForward}).text == "x");
  }

  // Word moves land past the next word and back at a word's start.
  {
    const TextEdit start = at("hello, big world", 0);
    TextEdit e = run(start, by(kRight, TextUnit::Word));
    assert(e.selection.start == 5);
    e = run(e, by(kRight, TextUnit::Word));
    assert(e.selection.start == 10);
    e = run(e, by(kRight, TextUnit::Word));
    assert(e.selection.start == 16);
    e = run(e, by(kLeft, TextUnit::Word));
    assert(e.selection.start == 11);
    e = run(e, by(kLeft, TextUnit::Word));
    assert(e.selection.start == 7);
    assert(run(e, by(kLeft, TextUnit::Word)).selection.start == 0);
    assert(run(at("привет мир", 0), by(kRight, TextUnit::Word)).selection.start == 12);
    // A selection moves on from its side.
    assert(
      run({.text = "hello world", .selection = {0, 5}}, by(kRight, TextUnit::Word))
        .selection.start == 11
    );
  }

  // Word deletes reach the same boundaries.
  {
    assert(
      run(at("hello big world", 15), by({.op = TextOp::DeleteBackward}, TextUnit::Word)).text ==
      "hello big "
    );
    assert(
      run(at("hello big world", 0), by({.op = TextOp::DeleteForward}, TextUnit::Word)).text ==
      " big world"
    );
  }

  // Lines: moves stop at the hard line's ends; deletes reach them, or one
  // cluster when already there.
  {
    const TextEdit mid = at("ab\ncd\nef", 4);
    assert(run(mid, by(kLeft, TextUnit::Line)).selection.start == 3);
    assert(run(mid, by(kRight, TextUnit::Line)).selection.start == 5);
    TextEdit e = run(at("ab\ncd\nef", 5), by({.op = TextOp::DeleteBackward}, TextUnit::Line));
    assert(e.text == "ab\n\nef" && e.selection.start == 3);
    e = run(at("ab\ncd", 3), by({.op = TextOp::DeleteBackward}, TextUnit::Line));
    assert(e.text == "abcd" && e.selection.start == 2);
    assert(run(at("ab\ncd", 0), by({.op = TextOp::DeleteForward}, TextUnit::Line)).text == "\ncd");
    assert(run(mid, by(kRight, TextUnit::Document)).selection.start == 8);
    assert(run(mid, by(kLeft, TextUnit::Document)).selection.start == 0);
  }

  // Extending moves only the selection's moving end, on either side.
  {
    TextEdit e = run(at("hello world", 5), by(kRight, TextUnit::Character, true));
    assert(sel(e, 5, 1) && !e.reversed);
    e = run(e, by(kRight, TextUnit::Word, true));
    assert(sel(e, 5, 6));
    e = run(e, by(kLeft, TextUnit::Character, true));
    assert(sel(e, 5, 5));
    e = run(at("hello world", 5), by(kLeft, TextUnit::Word, true));
    assert(sel(e, 0, 5) && e.reversed);
    e = run(e, by(kRight, TextUnit::Character, true));
    assert(sel(e, 1, 4) && e.reversed);
    e = run(e, by(kRight, TextUnit::Word, true));
    assert(sel(e, 5, 0));
    e = run(e, by(kRight, TextUnit::Character, true));
    assert(sel(e, 5, 1) && !e.reversed);
    e = run(at("ab\ncd", 4), by(kLeft, TextUnit::Document, true));
    assert(sel(e, 0, 4) && e.reversed);
  }

  // Select word takes the run at the caret: a word, spaces or symbols.
  {
    assert(sel(run(at("hello big world", 7), {.op = TextOp::SelectWord}), 6, 3));
    assert(sel(run(at("hello big world", 5), {.op = TextOp::SelectWord}), 5, 1));
    assert(sel(run(at("hello big world", 15), {.op = TextOp::SelectWord}), 10, 5));
    assert(sel(run(at("", 0), {.op = TextOp::SelectWord}), 0, 0));
    assert(sel(run(at("a, b", 1), {.op = TextOp::SelectWord}), 1, 1));
  }

  // Typing coalesces into one undo step until another command runs; undo
  // restores text and selection, and redo replays.
  {
    TextEdit e = type(type(at("", 0), "h"), "i");
    e = run(e, {.op = TextOp::Undo});
    assert(e.text.empty());
    e = run(e, {.op = TextOp::Redo});
    assert(e.text == "hi" && e.selection.start == 2);
    e = type(run(type(type(at("", 0), "a"), "b"), kLeft), "c");
    assert(e.text == "acb");
    e = run(e, {.op = TextOp::Undo});
    assert(e.text == "ab");
    assert(run(e, {.op = TextOp::Undo}).text.empty());
    e = run(at("abcd", 4), {.op = TextOp::DeleteBackward});
    e = run(e, {.op = TextOp::DeleteBackward});
    assert(e.text == "ab" && run(e, {.op = TextOp::Undo}).text == "abcd");
    const TextChange word = by({.op = TextOp::DeleteBackward}, TextUnit::Word);
    e = run(run(at("one two three", 13), word), word);
    assert(run(e, {.op = TextOp::Undo}).text == "one two ");
    e = type(run(at("abc", 0), {.op = TextOp::SelectAll}), "x");
    e = run(e, {.op = TextOp::Undo});
    assert(e.text == "abc" && sel(e, 0, 3));
    // A new edit drops redo; undo with nothing to undo changes nothing.
    e = type(run(type(at("", 0), "a"), {.op = TextOp::Undo}), "b");
    assert(run(e, {.op = TextOp::Redo}).text == "b");
    assert(run(at("q", 1), {.op = TextOp::Undo}).text == "q");
    // A composition joins the typing around it.
    e = type(at("", 0), "a");
    e = run(e, {.op = TextOp::Mark, .text = "n", .selection = {1, 0}});
    e = type(e, "ñ");
    assert(e.text == "añ" && run(e, {.op = TextOp::Undo}).text.empty());
    // History keeps a bounded depth.
    e = at("", 0);
    for (int i = 0; i < 150; ++i) e = run(type(e, "x"), kLeft);
    assert(e.history.undo.size() == 100);
  }

  // Rules: max length counts clusters, filters drop code points, and a
  // full field takes no edit and no undo step.
  {
    const TextRules three{.maxLength = 3};
    TextEdit e = run(at("", 0), {.text = "abcd"}, three);
    assert(e.text == "abc");
    const TextEdit full = run(e, {.text = "z"}, three);
    assert(full.text == "abc" && full.history.undo.size() == e.history.undo.size());
    assert(run({.text = "abc", .selection = {0, 1}}, {.text = "xy"}, three).text == "xbc");
    assert(run(at("", 0), {.text = kFamily + "ab"}, {.maxLength = 2}).text == kFamily + "a");
    assert(run(at("", 0), {.text = "1a2"}, {.accept = digits}).text == "12");
  }

  // Select sets the range a platform chose (UIKit's selection, a
  // selection handle); it is clamped and commits any composition.
  {
    TextEdit e = run(at("hello", 0), {.op = TextOp::Select, .selection = {1, 3}});
    assert(sel(e, 1, 3) && !e.reversed);
    assert(sel(run(at("hi", 0), {.op = TextOp::Select, .selection = {1, 9}}), 1, 1));
    assert(sel(run(at(kAcute, 0), {.op = TextOp::Select, .selection = {1, 0}}), 0, 0));
    assert(sel(run(at("x" + kAcute, 0), {.op = TextOp::Select, .selection = {0, 2}}), 0, 4));
    e = run(at("", 0), {.op = TextOp::Mark, .text = "n", .selection = {1, 0}});
    e = run(e, {.op = TextOp::Select, .selection = {0, 0}});
    assert(e.text == "n" && !e.marked && sel(e, 0, 0));
  }

  // Presses: one tap places the caret, two select the word, three select
  // all; taps far apart in time or space start over. Shift extends.
  {
    TextEdit e = pressText(at("hello big world", 0), 7, 50, 5, 1.f);
    assert(sel(e, 7, 0) && e.press.taps == 1);
    e = pressText(e, 7, 51, 5, 1.2f);
    assert(sel(e, 6, 3) && e.press.taps == 2);
    e = pressText(e, 7, 51, 5, 1.4f);
    assert(sel(e, 0, 15));
    assert(pressText(e, 7, 51, 5, 2.f).press.taps == 1);
    assert(pressText(pressText(at("ab", 0), 1, 0, 0, 1.f), 1, 40, 0, 1.1f).press.taps == 1);
    e = pressText(at("hello world", 5), 11, 0, 0, 1.f, true);
    assert(sel(e, 5, 6) && !e.reversed);
    e = pressText(e, 2, 0, 0, 1.1f, true);
    assert(sel(e, 2, 3) && e.reversed);
  }

  // Drags move the selection's moving end and keep the press as anchor.
  {
    TextEdit e = pressText(at("hello world", 0), 5, 0, 0, 1.f);
    e = dragText(e, 9);
    assert(sel(e, 5, 4) && !e.reversed);
    e = dragText(e, 1);
    assert(sel(e, 1, 4) && e.reversed);
    e = dragText(e, 5);
    assert(sel(e, 5, 0));
  }

  // Secure fields show a bullet per cluster and never copy.
  {
    const TextRules secure{.secure = true};
    const TextEdit e{.text = "a" + kThumb + "b", .selection = {0, 10}};
    assert(!copyText(e, secure));
    assert(copyText(e) == e.text && !copyText(at("ab", 1)));
    assert(shownText(e.text, secure) == "•••" && shownText(e.text, {}) == e.text);
    assert(shownOffset(e.text, 9, secure) == 6 && shownOffset(e.text, 9, {}) == 9);
  }
}
