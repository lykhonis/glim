#pragma once

#include <glim/shell/Clipboard.h>
#include <glim/toolkit/Block.h>
#include <glim/toolkit/Screen.h>
#include <glim/toolkit/TextEdit.h>

#include <functional>
#include <string_view>
#include <vector>

namespace glim::toolkit::host {

struct ClipboardAccess {
  shell::ClipboardText (*read)() = shell::readClipboard;
  shell::ClipboardStatus (*write)(std::string_view) = shell::writeClipboard;
};

// The actions for the focused field, fixed when the menu opens.
std::vector<TextAction> editMenuActions(
  const Screen& screen,
  const Block& tree,
  const Box& box,
  const Engine& engine,
  const ClipboardAccess& clipboard = {}
);
// Carries out a pick on the focused field; true when the field changed.
bool runEditAction(
  Screen& screen,
  const Block& tree,
  const Box& box,
  TextAction action,
  const Engine& engine,
  const ClipboardAccess& clipboard = {}
);
// The tree with the edit menu over it while screen.menuAt is set.
Block withEditMenu(
  Block tree,
  const Screen& screen,
  const std::vector<TextAction>& actions,
  std::function<void(TextAction)> pick
);

}  // namespace glim::toolkit::host
