#include <cassert>
#include <optional>
#include <string>
#include <vector>

#include "glim/toolkit/EditMenu.h"
#include "test_helpers.h"

using namespace glim::toolkit;

void test_edit_menu() {
  // The menu shows one labeled button per action, at its anchor, and a
  // press on one picks that action.
  std::optional<TextAction> picked;
  const std::vector<TextAction> actions{TextAction::Copy, TextAction::Paste};
  Block tree = stack(empty(), editMenu(30, 40, actions, [&](TextAction a) { picked = a; }));
  Engine e;
  const Box tb = e.place(tree, {0, 400, 0, 300});
  const std::vector<A11yNode> nodes = a11yNodes(tree, tb, e);
  std::vector<A11yNode> buttons;
  std::vector<std::string> labels;
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].traits.role == A11yRole::Button) buttons.push_back(nodes[i]);
    if (i > 0 && nodes[i - 1].traits.role == A11yRole::Button) labels.push_back(nodes[i].label);
  }
  assert(buttons.size() == 2);
  assert((labels == std::vector<std::string>{"Copy", "Paste"}));
  assert(buttons[0].box.x >= 30 && buttons[0].box.y >= 40);
  assert(buttons[1].box.x > buttons[0].box.x);
  Screen screen;
  const Box paste = buttons[1].box;
  assert(screen.pointerDown(tree, tb, paste.x + paste.w / 2, paste.y + paste.h / 2, 1, e));
  assert(picked == TextAction::Paste);
  assert(textActionLabel(TextAction::Cut) == "Cut");
  assert(textActionLabel(TextAction::SelectAll) == "Select All");

  // A right-click on a focused field opens the menu at the press and keeps
  // a selection it lands in; any other press closes it.
  {
    Store store;
    Engine fe;
    fe.store = store;
    State<TextEdit> value = rootScope(fe).state("menu", TextEdit{.text = "hello world"});
    const auto build = [&] {
      return column(textField({.value = value, .width = 300}), lines("x"), tap(lines("Go"), [] {}));
    };
    Block ft = build();
    const Box fb = fe.place(ft, {0, 400, 0, 300});
    Screen s;
    const Box field = focusBox(ft, fb, 0, fe);
    const float y = field.y + 4;
    assert(!s.contextPress(ft, fb, field.x + 4, field.y + field.h + 10, fe));
    assert(!s.menuAt);
    // A focusable that edits no text opens no menu.
    const Box go = focusBox(ft, fb, 1, fe);
    assert(!s.contextPress(ft, fb, go.x + 2, go.y + 2, fe) && !s.menuAt);
    value = TextEdit{.text = "hello world", .selection = {0, 5}};
    s.focus = 0;
    ft = build();
    const float inside = field.x + FieldGeometry::kPadX + 2 * Tokens::kTypeAdvance;
    assert(s.contextPress(ft, fb, inside, y, fe));
    assert(s.menuAt && s.menuAt->x == inside && s.menuAt->y == y);
    assert(value->selection.start == 0 && value->selection.length == 5);
    // Outside the selection it moves the caret first, like a press.
    ft = build();
    const float far = field.x + FieldGeometry::kPadX + 8 * Tokens::kTypeAdvance;
    s.clock.now = 10.f;
    assert(s.contextPress(ft, fb, far, y, fe));
    assert(value->selection.length == 0 && value->selection.start == 8);
    ft = build();
    s.clock.now = 20.f;
    s.pointerDown(ft, fb, far, y, 1, fe);
    s.pointerUp(ft, fb, far, y, fe);
    assert(!s.menuAt);
  }
}
