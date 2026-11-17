#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

struct Row : WithGap, WithCenter, WithPad {
  static constexpr const char* kKind = "Row";
  const char* kind() const { return kKind; }
  std::vector<Block> children;
  // Defaulted in Elements.cpp (Block is incomplete here).
  Row();
  ~Row();
  Row(const Row&);
  Row& operator=(const Row&);
  Row(Row&&);
  Row& operator=(Row&&);
  Box place(Constraints c, const Engine& e) const {
    Constraints ic = detail::inset(c, static_cast<float>(2 * padH), static_cast<float>(2 * padV));
    detail::Flow f = detail::flowRow(children, gap, ic, e);
    if (centered) detail::centerRow(f);
    f.size.w = detail::clampF(f.size.w + static_cast<float>(2 * padH), c.minW, c.maxW);
    f.size.h = detail::clampF(f.size.h + static_cast<float>(2 * padV), c.minH, c.maxH);
    return f.size;
  }
  // All child boxes from one flow; walks take these instead of childBox per child.
  std::vector<Box> childBoxes(const Box& box, const Engine& e) const {
    Box inner{
      box.x + static_cast<float>(padH),
      box.y + static_cast<float>(padV),
      box.w - static_cast<float>(2 * padH),
      box.h - static_cast<float>(2 * padV)
    };
    if (inner.w < 0) inner.w = 0;
    if (inner.h < 0) inner.h = 0;
    detail::Flow f = detail::flowRow(children, gap, {0, inner.w, 0, inner.h}, e);
    if (centered) detail::centerRow(f);
    return detail::offsetBoxes(std::move(f.children), inner.x, inner.y);
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    return childBoxes(box, e)[i];
  }
};

template <class... As>
auto row(As&&... as) {
  Row r;
  configure(r, r.children, std::forward<As>(as)...);
  return finish(std::move(r), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
