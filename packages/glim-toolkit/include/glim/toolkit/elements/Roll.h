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
  // Defaulted in Elements.cpp (Block is incomplete here).
  Roll();
  ~Roll();
  Roll(const Roll&);
  Roll& operator=(const Roll&);
  Roll(Roll&&);
  Roll& operator=(Roll&&);
  // Shared-handle write through a const view.
  void scrollTo(float v) const {
    if (scrollOff) scrollOff = v;
  }
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

// Scroll geometry in one home so routing and scrollbar agree.
struct RollMetrics {
  float content = 0.0f;
  float viewport = 0.0f;
  float offset = 0.0f;
  float maxOff = 0.0f;
  bool horizontal = false;
};

inline RollMetrics rollMetrics(const Roll& r, const Box& box, const Engine& e) {
  RollMetrics m;
  m.horizontal = r.axis == kAxisHorizontal;
  m.offset = r.scrollOff ? *r.scrollOff : 0.0f;
  if (m.horizontal) {
    detail::Flow f = detail::flowRow(r.children, r.gap, {0, kUnbounded, 0, box.h}, e);
    m.content = f.size.w;
    m.viewport = box.w;
  } else {
    detail::Flow f = detail::flowColumn(r.children, r.gap, {0, box.w, 0, kUnbounded}, e);
    m.content = f.size.h;
    m.viewport = box.h;
  }
  m.maxOff = m.content - m.viewport;
  return m;
}

}  // namespace toolkit
}  // namespace glim
