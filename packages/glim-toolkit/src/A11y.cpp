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
    const std::string* mark = semanticOf(n);
    bool focusable = canHandle(n);
    if (!mark && label.empty() && !focusable) return;
    std::string role = "Custom";
    if (!tmp.entries.empty()) role = tmp.entries.front().kind;
    out.push_back({mark ? *mark : std::string{}, std::move(role), std::move(label), bx, focusable});
  });
  return out;
}

const A11yNode* a11yFind(const std::vector<A11yNode>& nodes, const std::string& id) {
  for (const auto& n : nodes) {
    if (n.id == id) return &n;
  }
  return nullptr;
}

void dumpA11y(std::ostream& os, const std::vector<A11yNode>& nodes) {
  for (const auto& n : nodes) {
    os << n.role << " [" << n.box.x << "," << n.box.y << " " << n.box.w << "x" << n.box.h << "]";
    if (!n.label.empty()) os << " " << n.label;
    if (!n.id.empty()) os << " " << n.id;
    if (n.focusable) os << " *";
    os << "\n";
  }
}

}  // namespace toolkit
}  // namespace glim
