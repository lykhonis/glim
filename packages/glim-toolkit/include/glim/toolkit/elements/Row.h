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
  Box place(Constraints c, const Engine& e) const {
    Constraints ic = detail::inset(c, static_cast<float>(2 * padH), static_cast<float>(2 * padV));
    detail::Flow f = detail::flowRow(children, gap, ic, e);
    if (centered) detail::centerRow(f);
    f.size.w = detail::clampF(f.size.w + static_cast<float>(2 * padH), c.minW, c.maxW);
    f.size.h = detail::clampF(f.size.h + static_cast<float>(2 * padV), c.minH, c.maxH);
    return f.size;
  }
  template <class F>
  void eachChild(F&& f) const {
    detail::forEachBlock(children, std::forward<F>(f));
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
    detail::Flow f = detail::flowRow(children, gap, {0, inner.w, 0, inner.h}, e);
    if (centered) detail::centerRow(f);
    Box b = f.children[i];
    b.x += inner.x;
    b.y += inner.y;
    return b;
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
