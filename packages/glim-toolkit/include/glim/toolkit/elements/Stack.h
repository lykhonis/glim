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
  const char* kind() const { return kKind; }
  std::vector<Block> children;
  Box place(Constraints c, const Engine& e) const { return detail::flowStack(children, c, e).size; }
  template <class F>
  void eachChild(F&& f) const {
    detail::forEachBlock(children, std::forward<F>(f));
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    detail::Flow f = detail::flowStack(children, {0, box.w, 0, box.h}, e);
    Box b = f.children[i];
    b.x += box.x;
    b.y += box.y;
    return b;
  }
  void record(const Box& box, Rec& rec, const Engine& e) const {
    rec.push(kKind, box);
    for (std::size_t i = 0; i < children.size(); ++i)
      e.record(children[i], childBox(i, box, e), rec);
  }
};

template <class... As>
auto stack(As&&... as) {
  Stack s;
  configure(s, s.children, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
