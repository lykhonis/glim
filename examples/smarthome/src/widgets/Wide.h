#pragma once

// Wide: transparent stretch wrapper. Forces its single child to the
// full available width (column cross-axis does not stretch by
// itself). Paint is box-driven (Bars, StripChart) so children
// re-flow; rows use expand() children to split the forced width.
#include <cstddef>
#include <utility>
#include <vector>

#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Wide : WithSemantic {
  static constexpr const char* kKind = "Wide";
  std::vector<Block> children;
  Box place(Constraints c, const Engine& e) const {
    if (children.empty()) return detail::resolveSize(0, 0, c);
    if (isUnbounded(c.maxW)) {
      detail::Flow f = detail::flowColumn(children, 0, c, e);
      return f.size;
    }
    float w = c.maxW;
    Box b = e.place(children[0], {w, w, 0, c.maxH});
    b.x = 0;
    b.y = 0;
    return {0, 0, w, b.h};
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    if (i >= children.size()) return box;
    Box b = e.place(children[i], {box.w, box.w, 0, box.h});
    b.x = box.x;
    b.y = box.y;
    return b;
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 0);
  }
  void paint(const Box&, AnySink&, const Engine&) const {}
};

template <class... As>
auto wide(As&&... as) {
  Wide w;
  configure(w, w.children, std::forward<As>(as)...);
  return finish(std::move(w), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
