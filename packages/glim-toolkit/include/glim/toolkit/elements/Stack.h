#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

struct Stack {
  static constexpr const char* kKind = "Stack";
  std::vector<Block> children;
  Box place(Constraints c, const Engine& e) const {
    return detail::flowStack(children, c, e).size;
  }
  void record(const Box& box, Rec& rec, const Engine& e) const {
    rec.push(kKind, box);
    detail::Flow f = detail::flowStack(children, {0, box.w, 0, box.h}, e);
    for (std::size_t i = 0; i < children.size(); ++i) {
      Box b = f.children[i];
      b.x += box.x;
      b.y += box.y;
      e.record(children[i], b, rec);
    }
  }
};

template <class... As>
auto stack(As&&... as) {
  Stack s;
  configure(s, s.children, std::forward<As>(as)...);
  return s;
}

}  // namespace toolkit
}  // namespace glim
