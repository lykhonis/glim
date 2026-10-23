#pragma once

#include <utility>
#include <vector>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

struct Fill {
  static constexpr const char* kKind = "Fill";
  std::vector<Block> children;
  Box place(Constraints c, const Engine& e) const;
  void record(const Box& box, Rec& rec, const Engine& e) const;
};

template <class A>
Fill fill(A&& a) {
  Fill f;
  f.children.push_back(toBlock(std::forward<A>(a)));
  return f;
}

inline Box Fill::place(Constraints c, const Engine& e) const {
  Box inner = e.place(children[0], {0, c.maxW, 0, c.maxH});
  float w = inner.w;
  float h = inner.h;
  if (!isUnbounded(c.maxW) && c.maxW > w) w = c.maxW;
  if (!isUnbounded(c.maxH) && c.maxH > h) h = c.maxH;
  w = detail::clampF(w, c.minW, c.maxW);
  h = detail::clampF(h, c.minH, c.maxH);
  return {0, 0, w, h};
}

inline void Fill::record(const Box& box, Rec& rec, const Engine& e) const {
  rec.push(kKind, box);
  e.record(children[0], box, rec);
}

}  // namespace toolkit
}  // namespace glim
