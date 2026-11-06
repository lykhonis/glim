#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

// Eager v1: all items laid out; virtualization later.
struct List : WithGap {
  static constexpr const char* kKind = "List";
  const char* kind() const { return kKind; }
  std::vector<Block> children;
  // Defaulted in Elements.cpp (Block is incomplete here).
  List();
  ~List();
  List(const List&);
  List& operator=(const List&);
  List(List&&);
  List& operator=(List&&);
  Box place(Constraints c, const Engine& e) const {
    return detail::flowColumn(children, gap, c, e).size;
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    detail::Flow f = detail::flowColumn(children, gap, {0, box.w, 0, box.h}, e);
    Box b = f.children[i];
    b.x += box.x;
    b.y += box.y;
    return b;
  }
};

template <class F, class... As>
auto list(int count, F&& fn, As&&... as) {
  List l;
  if (count < 0) count = 0;
  for (int i = 0; i < count; ++i) l.children.push_back(toBlock(fn(i)));
  configureTags(l, std::forward<As>(as)...);
  return finish(std::move(l), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
