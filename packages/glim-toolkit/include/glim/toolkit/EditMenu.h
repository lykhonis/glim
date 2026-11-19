#pragma once

#include <functional>
#include <string_view>
#include <vector>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/TextEdit.h"

namespace glim {
namespace toolkit {

std::string_view textActionLabel(TextAction action);

// The edit menu Glim draws where the platform has none: a glass row of one
// button per action, its top left at (x, y). Pressing a button picks it.
Block editMenu(
  float x, float y, const std::vector<TextAction>& actions, std::function<void(TextAction)> pick
);

}  // namespace toolkit
}  // namespace glim
