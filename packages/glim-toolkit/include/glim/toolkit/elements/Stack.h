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
  // Defaulted in Elements.cpp (Block is incomplete here).
  Stack();
  ~Stack();
  Stack(const Stack&);
  Stack& operator=(const Stack&);
  Stack(Stack&&);
  Stack& operator=(Stack&&);
  Box place(Constraints c, const Engine& e) const { return detail::flowStack(children, c, e).size; }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    detail::Flow f = detail::flowStack(children, {0, box.w, 0, box.h}, e);
    Box b = f.children[i];
    b.x += box.x;
    b.y += box.y;
    return b;
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
