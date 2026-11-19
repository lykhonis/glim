#include "glim/toolkit/host/EditMenu.h"

#include <glim/toolkit.h>

#include <string>
#include <utility>

namespace glim::toolkit::host {

std::vector<TextAction> editMenuActions(
  const Screen& screen,
  const Block& tree,
  const Box& box,
  const Engine& engine,
  const ClipboardAccess& clipboard
) {
  const std::optional<TextFocus> focus = screen.textFocus(tree, box, engine);
  if (!focus) return {};
  const bool canPaste = clipboard.read().status == shell::ClipboardStatus::Ok;
  return textActions(focus->edit, focus->rules, canPaste);
}

bool runEditAction(
  Screen& screen,
  const Block& tree,
  const Box& box,
  TextAction action,
  const Engine& engine,
  const ClipboardAccess& clipboard
) {
  const std::optional<TextFocus> focus = screen.textFocus(tree, box, engine);
  if (!focus) return false;
  std::string pasted;
  if (action == TextAction::Paste) {
    shell::ClipboardText read = clipboard.read();
    if (read.status != shell::ClipboardStatus::Ok) return false;
    pasted = std::move(read.text);
  }
  const TextActionStep step = textActionStep(action, focus->edit, focus->rules, pasted);
  if (step.copy) clipboard.write(*step.copy);
  return step.change && screen.text(tree, box, *step.change, engine);
}

Block withEditMenu(
  Block tree,
  const Screen& screen,
  const std::vector<TextAction>& actions,
  std::function<void(TextAction)> pick
) {
  if (!screen.menuAt) return tree;
  return stack(
    std::move(tree), editMenu(screen.menuAt->x, screen.menuAt->y, actions, std::move(pick))
  );
}

}  // namespace glim::toolkit::host
