#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

struct Row : WithGap {
  static constexpr const char* kKind = "Row";
  std::vector<Block> children;
  Box place(Constraints c, const Engine& e) const {
    return detail::flowRow(children, gap, c, e).size;
  }
  void record(const Box& box, Rec& rec, const Engine& e) const {
    rec.push(kKind, box);
    detail::Flow f = detail::flowRow(children, gap, {0, box.w, 0, box.h}, e);
    for (std::size_t i = 0; i < children.size(); ++i) {
      Box b = f.children[i];
      b.x += box.x;
      b.y += box.y;
      e.record(children[i], b, rec);
    }
  }
};

template <class... As>
auto row(As&&... as) {
  Row r;
  configure(r, r.children, std::forward<As>(as)...);
  return r;
}

}  // namespace toolkit
}  // namespace glim
