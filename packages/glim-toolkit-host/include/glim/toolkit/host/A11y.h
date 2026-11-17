#pragma once

#include <glim/shell/A11y.h>

#include <functional>
#include <utility>
#include <vector>

#include "glim/toolkit/A11y.h"
#include "glim/toolkit/Screen.h"

namespace glim {
namespace toolkit {
namespace host {

// Toolkit nodes to the shell's OS a11y elements, index for index; `focus`
// is Screen::focus.
std::vector<shell::A11yElement> shellA11y(const std::vector<A11yNode>& nodes, int focus = -1);

// Fresh tree and its box; activation rebuilds between down and up.
using A11yBuild = std::function<std::pair<Block, Box>()>;

// A screen reader's command on `node`, delivered as the input it stands
// for: a tap at the frame's centre, an Adjust step there, or a focus move.
// True when the screen changed.
bool performA11y(
  Screen& screen,
  const A11yNode& node,
  shell::A11yCommand command,
  const A11yBuild& build,
  const Engine& e = Engine{}
);

}  // namespace host
}  // namespace toolkit
}  // namespace glim
