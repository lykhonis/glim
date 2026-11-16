#pragma once

#include <string>
#include <vector>

#include "glim/toolkit/A11yTraits.h"
#include "glim/toolkit/Layout.h"

namespace glim {
namespace toolkit {

// Toolkit-side semantics for Host OS bridges; strings owned.
struct A11yNode {
  std::string id;
  std::string kind;
  std::string label;
  Box box;
  bool focusable = false;
  A11yTraits traits;
  // Index of the enclosing node in the same list; -1 at the top.
  int parent = -1;
};

// An embedded component's own subtree: boxes in its space (0..width,
// 0..height), parents indexing this list. The walk grafts it under the host.
struct A11yExport {
  float width = 0;
  float height = 0;
  std::vector<A11yNode> nodes;
};

}  // namespace toolkit
}  // namespace glim
