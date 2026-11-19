#include "glim/toolkit/EditMenu.h"

#include <string>
#include <utility>

#include "glim/toolkit/Toolkit.h"

namespace glim {
namespace toolkit {

std::string_view textActionLabel(TextAction action) {
  switch (action) {
    case TextAction::Cut:
      return "Cut";
    case TextAction::Copy:
      return "Copy";
    case TextAction::Paste:
      return "Paste";
    case TextAction::SelectAll:
      return "Select All";
  }
  return {};
}

Block editMenu(
  float x, float y, const std::vector<TextAction>& actions, std::function<void(TextAction)> pick
) {
  Row items;
  for (TextAction a : actions) {
    items.children.push_back(tap(
      lines(std::string(textActionLabel(a))), [pick, a] { pick(a); }, pad(10, 6)
    ));
  }
  return finish(std::move(items), glass(), translate(x, y));
}

}  // namespace toolkit
}  // namespace glim
