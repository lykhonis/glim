#include <glim/toolkit/host/A11y.h>

#include <sstream>

namespace glim {
namespace toolkit {
namespace host {

namespace {

shell::A11yRole roleOf(const A11yNode& n) {
  switch (n.traits.role) {
    case A11yRole::None:
      return n.label.empty() ? shell::A11yRole::Group : shell::A11yRole::Text;
    case A11yRole::Text:
      return shell::A11yRole::Text;
    case A11yRole::Heading:
      return shell::A11yRole::Heading;
    case A11yRole::Button:
      return shell::A11yRole::Button;
    case A11yRole::Switch:
      return shell::A11yRole::Switch;
    case A11yRole::Slider:
      return shell::A11yRole::Slider;
    case A11yRole::TextField:
      return shell::A11yRole::TextField;
  }
  return shell::A11yRole::Group;
}

std::string valueOf(const A11yTraits& t) {
  if (!t.value.empty() || !t.range) return t.value;
  std::ostringstream os;
  os << t.range->now;
  return os.str();
}

}  // namespace

std::vector<shell::A11yElement> shellA11y(const std::vector<A11yNode>& nodes, int focus) {
  std::vector<shell::A11yElement> out;
  out.reserve(nodes.size());
  for (const A11yNode& n : nodes) {
    out.push_back({
      .role = roleOf(n),
      .label = n.label.empty() ? n.id : n.label,
      .value = valueOf(n.traits),
      .frame = Rect{{n.box.x, n.box.y}, {n.box.w, n.box.h}},
      .parent = n.parent,
      .checked = n.traits.states.has(A11yState::Checked),
      .disabled = n.traits.states.has(A11yState::Disabled),
      .adjustable =
        n.traits.actions.has(A11yAction::Increment) || n.traits.actions.has(A11yAction::Decrement),
      .focused = focus >= 0 && n.focusIndex == focus,
    });
  }
  return out;
}

bool performA11y(
  Screen& screen,
  const A11yNode& node,
  shell::A11yCommand command,
  const A11yBuild& build,
  const Engine& e
) {
  if (command == shell::A11yCommand::Focus) {
    if (node.focusIndex < 0 || node.focusIndex == screen.focus) return false;
    screen.focus = node.focusIndex;
    screen.focusVisible = true;
    return true;
  }
  const float x = node.box.x + node.box.w * 0.5f;
  const float y = node.box.y + node.box.h * 0.5f;
  if (command != shell::A11yCommand::Activate) {
    const Adjust adjust{command == shell::A11yCommand::Increment ? 1 : -1};
    const auto [tree, box] = build();
    return dispatchPointer(tree, box, x, y, Event::borrow(adjust), e);
  }
  const auto [down, downBox] = build();
  const bool pressed = screen.pointerDown(down, downBox, x, y, 0, e);
  const auto [up, upBox] = build();
  return screen.pointerUp(up, upBox, x, y, e) || pressed;
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
