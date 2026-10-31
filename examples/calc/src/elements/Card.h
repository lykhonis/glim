#pragma once

#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {

struct Card : WithGap, WithRadius, WithThrough, WithPad, WithSemantic {
  static constexpr const char* kKind = "Card";
  std::vector<Block> children;
  Box place(Constraints c, const Engine& e) const {
    Constraints ic = detail::inset(
        c, static_cast<float>(2 * padH), static_cast<float>(2 * padV)
    );
    detail::Flow f = detail::flowColumn(children, gap, ic, e);
    f.size.w = detail::clampF(
        f.size.w + static_cast<float>(2 * padH), c.minW, c.maxW
    );
    f.size.h = detail::clampF(
        f.size.h + static_cast<float>(2 * padV), c.minH, c.maxH
    );
    return f.size;
  }
  template <class F>
  void eachChild(F&& f) const {
    for (const auto& child : children) f(child);
  }
  Box innerBox(const Box& box) const {
    float w = box.w - static_cast<float>(2 * padH);
    float h = box.h - static_cast<float>(2 * padV);
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    return {
        box.x + static_cast<float>(padH),
        box.y + static_cast<float>(padV),
        w,
        h
    };
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    Box inner = innerBox(box);
    detail::Flow f =
        detail::flowColumn(children, gap, {0, inner.w, 0, inner.h}, e);
    Box b = f.children[i];
    b.x += inner.x;
    b.y += inner.y;
    return b;
  }
  void record(const Box& box, Rec& rec, const Engine& e) const {
    rec.push(kKind, box, {}, semantic);
    for (std::size_t i = 0; i < children.size(); ++i)
      e.record(children[i], childBox(i, box, e), rec);
  }
};

template <class... As>
Block card(As&&... as) {
  Card c;
  c.padH = Tokens::kPadCard;
  c.padV = Tokens::kPadCard;
  configure(c, c.children, std::forward<As>(as)...);
  assert(!c.children.empty());
  return finish(std::move(c), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
