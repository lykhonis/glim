#pragma once

// Frosted duo from toolkit primitives. Order matters: the orb comes
// first so both glass groups stay consecutive (veil, glass, veil,
// glass, then content), which is what the rasterizer's
// ([backdrop][glass][overlay]) rule allows.

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Layout.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

// Tight veil around content: the box IS the veil, so the slot-tag pill
// coincides with the fill. Content is fixed at 240px so place/paint
// agree.
struct VeilCard : WithGap, WithSemantic {
  static constexpr const char* kKind = "VeilCard";
  const char* kind() const { return kKind; }
  std::vector<Block> children;
  Box place(Constraints c, const Engine& e) const {
    detail::Flow f = detail::flowColumn(
        children, gap, {0, 240.0f, 0, kUnbounded}, e
    );
    return detail::resolveSize(
        f.size.w + kChrome * 2.0f, f.size.h + kChrome * 2.0f, c
    );
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    detail::Flow f = detail::flowColumn(
        children, gap, {0, 240.0f, 0, kUnbounded}, e
    );
    Box b = f.children[i];
    b.x += box.x + kChrome;
    b.y += box.y + kChrome;
    return b;
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {});
  }
  void paint(const Box&, AnySink&, const Engine&) const {
    // No fills: the frost plate (masked by this box's pill) carries the
    // look.
  }
  static constexpr float kChrome = 24.0f;
};

template <class... As>
auto veilCard(As&&... as) {
  VeilCard v;
  configure(v, v.children, std::forward<As>(as)...);
  return finish(std::move(v), std::forward<As>(as)...);
}

// Fixed glass circle: fills its box, so the slot-tag pill coincides.
struct VeilOrb : WithSemantic {
  static constexpr const char* kKind = "VeilOrb";
  const char* kind() const { return kKind; }
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(kDiameter, kDiameter, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {});
  }
  void paint(const Box&, AnySink&, const Engine&) const {
    // No fills: the frost plate carries the look.
  }
  static constexpr float kDiameter = 72.0f;
};

template <class... As>
auto veilOrb(As&&... as) {
  VeilOrb v;
  return finish(std::move(v), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
