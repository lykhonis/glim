#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

struct Column : WithGap {
  static constexpr const char* kKind = "Column";
  const char* kind() const { return kKind; }
  std::vector<Block> children;
  Box place(Constraints c, const Engine& e) const {
    return detail::flowColumn(children, gap, c, e).size;
  }
  template <class F>
  void eachChild(F&& f) const {
    detail::forEachBlock(children, std::forward<F>(f));
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    detail::Flow f = detail::flowColumn(children, gap, {0, box.w, 0, box.h}, e);
    Box b = f.children[i];
    b.x += box.x;
    b.y += box.y;
    return b;
  }
};

template <class... As>
auto column(As&&... as) {
  Column c;
  configure(c, c.children, std::forward<As>(as)...);
  return finish(std::move(c), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
