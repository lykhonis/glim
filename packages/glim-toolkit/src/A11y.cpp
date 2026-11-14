#include "glim/toolkit/A11y.h"

#include <ostream>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/Walk.h"

namespace glim {
namespace toolkit {

std::vector<A11yNode> a11yNodes(const Block& root, const Box& box, const Engine& e) {
  std::vector<A11yNode> out;
  walk(root, box, e, [&](const Block& n, const Box& bx) {
    Rec tmp;
    describeSelf(n, bx, tmp);
    std::string label;
    for (const auto& en : tmp.entries) {
      if (!en.text.empty()) {
        label = en.text;
        break;
      }
    }
    auto mark = semanticOf(n);
    bool focusable = canHandle(n);
    A11yTraits traits = a11yOf(n);
    if (!mark && label.empty() && !focusable && traits.live == A11yLive::Off) return;
    if (focusable && traits.actions.empty()) traits.actions = {A11yAction::Activate};
    std::string kind = "Custom";
    if (!tmp.entries.empty()) kind = tmp.entries.front().kind;
    out.push_back(
      {mark ? mark->get() : std::string{},
       std::move(kind),
       std::move(label),
       bx,
       focusable,
       std::move(traits)}
    );
  });
  return out;
}

std::optional<std::reference_wrapper<const A11yNode>> a11yFind(
  const std::vector<A11yNode>& nodes, const std::string& id
) {
  for (const auto& n : nodes) {
    if (n.id == id) return std::cref(n);
  }
  return std::nullopt;
}

namespace {

const char* nameOf(A11yRole r) {
  switch (r) {
    case A11yRole::None:
      return "none";
    case A11yRole::Text:
      return "text";
    case A11yRole::Heading:
      return "heading";
    case A11yRole::Button:
      return "button";
    case A11yRole::Switch:
      return "switch";
    case A11yRole::Slider:
      return "slider";
    case A11yRole::TextField:
      return "textfield";
  }
  return "?";
}

const char* nameOf(A11yState s) {
  switch (s) {
    case A11yState::Checkable:
      return "checkable";
    case A11yState::Checked:
      return "checked";
    case A11yState::Selected:
      return "selected";
    case A11yState::Disabled:
      return "disabled";
  }
  return "?";
}

const char* nameOf(A11yAction a) {
  switch (a) {
    case A11yAction::Activate:
      return "activate";
    case A11yAction::Increment:
      return "increment";
    case A11yAction::Decrement:
      return "decrement";
  }
  return "?";
}

template <class E>
void dumpSet(std::ostream& os, const char* key, A11ySet<E> set) {
  const char* sep = key;
  for (std::uint32_t i = 0; i < 32; ++i) {
    if (!set.has(static_cast<E>(i))) continue;
    os << sep << nameOf(static_cast<E>(i));
    sep = ",";
  }
}

void dumpTraits(std::ostream& os, const A11yTraits& t) {
  if (!t.value.empty()) os << " value=" << t.value;
  if (t.range) os << " range=" << t.range->now << "/" << t.range->min << ".." << t.range->max;
  dumpSet(os, " states=", t.states);
  if (t.live != A11yLive::Off)
    os << (t.live == A11yLive::Polite ? " live=polite" : " live=assertive");
  dumpSet(os, " actions=", t.actions);
}

}  // namespace

void dumpA11y(std::ostream& os, const std::vector<A11yNode>& nodes) {
  for (const auto& n : nodes) {
    os << nameOf(n.traits.role) << " " << n.kind << " [" << n.box.x << "," << n.box.y << " "
       << n.box.w << "x" << n.box.h << "]";
    if (!n.label.empty()) os << " " << n.label;
    if (!n.id.empty()) os << " " << n.id;
    if (n.focusable) os << " *";
    dumpTraits(os, n.traits);
    os << "\n";
  }
}

}  // namespace toolkit
}  // namespace glim
