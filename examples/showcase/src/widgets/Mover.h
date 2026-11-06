#pragma once

#include <utility>
#include <vector>

#include "../motion/Anim.h"
#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"

namespace glim {
namespace toolkit {

// Offset wrapper: shifts its child by an animated amount. Layout, hit,
// paint, and a11y all follow childBox, so the shifted child stays
// consistent.
struct MoverProps {
  Anim<float> shift;
  bool vertical = false;
};

struct Mover {
  static constexpr const char* kKind = "Mover";
  const char* kind() const { return kKind; }
  std::vector<Block> children;
  Anim<float> shift;
  bool vertical = false;
  Box place(Constraints c, const Engine& e) const {
    if (children.empty()) return detail::resolveSize(0.0f, 0.0f, c);
    return e.place(children[0], c);
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    if (i != 0 || children.empty()) return box;
    Box b = e.place(children[0], {0, box.w, 0, box.h});
    b.x = box.x;
    b.y = box.y;
    if (vertical) {
      b.y += *shift;
    } else {
      b.x += *shift;
    }
    return b;
  }
};

template <class... As>
Block mover(MoverProps p, As&&... as) {
  Mover m;
  m.shift = std::move(p.shift);
  m.vertical = p.vertical;
  configure(m, m.children, std::forward<As>(as)...);
  assert(m.children.size() == 1);
  return finish(std::move(m), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
