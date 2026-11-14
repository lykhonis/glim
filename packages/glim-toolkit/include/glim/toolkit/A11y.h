#pragma once

#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "glim/toolkit/A11yTraits.h"
#include "glim/toolkit/Layout.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

struct Block;
struct Engine;

// Toolkit-side semantics for Host OS bridges; strings owned.
struct A11yNode {
  std::string id;
  std::string kind;
  std::string label;
  Box box;
  bool focusable = false;
  A11yTraits traits;
};

// Pre-order map; layout-only wrappers stay out.
std::vector<A11yNode> a11yNodes(const Block& root, const Box& box, const Engine& e = Engine{});

// First node with id, or nullptr. Duplicate ids resolve pre-order.
std::optional<std::reference_wrapper<const A11yNode>> a11yFind(
  const std::vector<A11yNode>& nodes, const std::string& id
);

// Text lowering for goldens.
void dumpA11y(std::ostream& os, const std::vector<A11yNode>& nodes);

}  // namespace toolkit
}  // namespace glim
