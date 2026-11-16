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

std::vector<shell::A11yElement> shellA11y(const std::vector<A11yNode>& nodes) {
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
    });
  }
  return out;
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
