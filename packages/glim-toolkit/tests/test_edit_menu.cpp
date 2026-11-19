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
}
