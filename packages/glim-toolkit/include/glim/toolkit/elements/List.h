#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

// Homogeneous repeated region. Eager v1: all items laid out like a `Column`;
// composes with `Roll` for scrolling. Virtualization later.
// Factories take (count, fn) — fn(i) returns a Block/viewable/element.
struct List : WithGap {
  static constexpr const char* kKind = "List";
  const char* kind() const { return kKind; }
  std::vector<Block> items;
  Box place(Constraints c, const Engine& e) const {
    return detail::flowColumn(items, gap, c, e).size;
  }
  template <class F>
  void eachChild(F&& f) const {
    detail::forEachBlock(items, std::forward<F>(f));
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    detail::Flow f = detail::flowColumn(items, gap, {0, box.w, 0, box.h}, e);
    Box b = f.children[i];
    b.x += box.x;
    b.y += box.y;
    return b;
  }
  void record(const Box& box, Rec& rec, const Engine& e) const {
    rec.push(kKind, box);
    for (std::size_t i = 0; i < items.size(); ++i) e.record(items[i], childBox(i, box, e), rec);
  }
};

template <class F, class... As>
List list(int count, F&& fn, As&&... as) {
  List l;
  if (count < 0) count = 0;
  for (int i = 0; i < count; ++i) l.items.push_back(toBlock(fn(i)));
  configureTags(l, std::forward<As>(as)...);
  return l;
}

}  // namespace toolkit
}  // namespace glim
