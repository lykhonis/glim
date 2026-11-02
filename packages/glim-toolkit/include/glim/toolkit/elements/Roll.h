#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

struct Roll : WithGap, WithAxis, WithScrollOffset {
  static constexpr const char* kKindH = "Roll.h";
  static constexpr const char* kKindV = "Roll.v";
  static constexpr bool kClipsChildren = true;
  const char* kind() const { return axis == kAxisHorizontal ? kKindH : kKindV; }
  std::vector<Block> children;
  Box place(Constraints c, const Engine& e) const {
    if (axis == kAxisHorizontal) {
      Constraints content{0, kUnbounded, 0, c.maxH};
      detail::Flow f = detail::flowRow(children, gap, content, e);
      return {
          0, 0, detail::clampF(f.size.w, c.minW, c.maxW), detail::clampF(f.size.h, c.minH, c.maxH)
      };
    }
    Constraints content{0, c.maxW, 0, kUnbounded};
    detail::Flow f = detail::flowColumn(children, gap, content, e);
    return {
        0, 0, detail::clampF(f.size.w, c.minW, c.maxW), detail::clampF(f.size.h, c.minH, c.maxH)
    };
  }
  template <class F>
  void eachChild(F&& f) const {
    detail::forEachBlock(children, std::forward<F>(f));
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    float off = scrollOff ? *scrollOff : 0.0f;
    if (axis == kAxisHorizontal) {
      detail::Flow f = detail::flowRow(children, gap, {0, kUnbounded, 0, box.h}, e);
      Box b = f.children[i];
      b.x += box.x - off;
      b.y += box.y;
      return b;
    }
    detail::Flow f = detail::flowColumn(children, gap, {0, box.w, 0, kUnbounded}, e);
    Box b = f.children[i];
    b.x += box.x;
    b.y += box.y - off;
    return b;
  }
};

template <class... As>
auto roll(As&&... as) {
  Roll r;
  configure(r, r.children, std::forward<As>(as)...);
  return finish(std::move(r), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
