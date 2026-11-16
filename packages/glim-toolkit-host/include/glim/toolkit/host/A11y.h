#pragma once

#include <glim/shell/A11y.h>

#include <vector>

#include "glim/toolkit/A11y.h"

namespace glim {
namespace toolkit {
namespace host {

// Toolkit nodes to the shell's OS a11y elements, index for index; `focus`
// is Screen::focus.
std::vector<shell::A11yElement> shellA11y(const std::vector<A11yNode>& nodes, int focus = -1);

}  // namespace host
}  // namespace toolkit
}  // namespace glim
