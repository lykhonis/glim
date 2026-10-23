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
  std::vector<Block> children;
  Box place(Constraints c, const Engine& e) const {
    if (axis == kAxisHorizontal) {
      Constraints content{0, kUnbounded, 0, c.maxH};
      detail::Flow f = detail::flowRow(children, gap, content, e);
      return {0, 0, detail::clampF(f.size.w, c.minW, c.maxW),
              detail::clampF(f.size.h, c.minH, c.maxH)};
    }
    Constraints content{0, c.maxW, 0, kUnbounded};
    detail::Flow f = detail::flowColumn(children, gap, content, e);
    return {0, 0, detail::clampF(f.size.w, c.minW, c.maxW),
            detail::clampF(f.size.h, c.minH, c.maxH)};
  }
  void record(const Box& box, Rec& rec, const Engine& e) const {
    rec.push(axis == kAxisHorizontal ? kKindH : kKindV, box);
    float off = scrollOff ? *scrollOff : 0.0f;
    if (axis == kAxisHorizontal) {
      Constraints content{0, kUnbounded, 0, box.h};
      detail::Flow f = detail::flowRow(children, gap, content, e);
      for (std::size_t i = 0; i < children.size(); ++i) {
        Box b = f.children[i];
        b.x += box.x - off;
        b.y += box.y;
        e.record(children[i], b, rec);
      }
      return;
    }
    Constraints content{0, box.w, 0, kUnbounded};
    detail::Flow f = detail::flowColumn(children, gap, content, e);
    for (std::size_t i = 0; i < children.size(); ++i) {
      Box b = f.children[i];
      b.x += box.x;
      b.y += box.y - off;
      e.record(children[i], b, rec);
    }
  }
};

template <class... As>
auto roll(As&&... as) {
  Roll r;
  configure(r, r.children, std::forward<As>(as)...);
  return r;
}

}  // namespace toolkit
}  // namespace glim
