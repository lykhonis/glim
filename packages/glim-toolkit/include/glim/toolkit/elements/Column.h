#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

struct Column : WithGap, WithCenter, WithPad {
  static constexpr const char* kKind = "Column";
  const char* kind() const { return kKind; }
  std::vector<Block> children;
  // Defaulted in Elements.cpp (Block is incomplete here).
  Column();
  ~Column();
  Column(const Column&);
  Column& operator=(const Column&);
  Column(Column&&);
  Column& operator=(Column&&);
  Box place(Constraints c, const Engine& e) const {
    Constraints ic = detail::inset(c, static_cast<float>(2 * padH), static_cast<float>(2 * padV));
    detail::Flow f = detail::flowColumn(children, gap, ic, e);
    if (centered) detail::centerColumn(f);
    f.size.w = detail::clampF(f.size.w + static_cast<float>(2 * padH), c.minW, c.maxW);
    f.size.h = detail::clampF(f.size.h + static_cast<float>(2 * padV), c.minH, c.maxH);
    return f.size;
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    Box inner{
      box.x + static_cast<float>(padH),
      box.y + static_cast<float>(padV),
      box.w - static_cast<float>(2 * padH),
      box.h - static_cast<float>(2 * padV)
    };
    if (inner.w < 0) inner.w = 0;
    if (inner.h < 0) inner.h = 0;
    detail::Flow f = detail::flowColumn(children, gap, {0, inner.w, 0, inner.h}, e);
    if (centered) detail::centerColumn(f);
    Box b = f.children[i];
    b.x += inner.x;
    b.y += inner.y;
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
