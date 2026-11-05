#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "glim/toolkit/Layout.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

struct Block;
struct Engine;

// One a11y node: toolkit-side semantics for Host OS bridges. role is
// the entry kind ("Tap", "Seat", "Lines", "Button", ...); label is the
// node's own entry text; id is the semantic mark. focusable mirrors
// focusables() membership (canHandle). Strings owned — safe to retain.
struct A11yNode {
  std::string id;
  std::string role;
  std::string label;
  Box box;
  bool focusable = false;
};

// Pre-order node map. Emits marked, labeled, and operable nodes;
// layout-only wrappers stay out. Bridges map role to OS roles later.
std::vector<A11yNode> a11yNodes(const Block& root, const Box& box, const Engine& e = Engine{});

// First node with id, or nullptr. Duplicate ids resolve pre-order.
const A11yNode* a11yFind(const std::vector<A11yNode>& nodes, const std::string& id);

// Text lowering for goldens: `role [x,y wxh] label id *`.
void dumpA11y(std::ostream& os, const std::vector<A11yNode>& nodes);

}  // namespace toolkit
}  // namespace glim
