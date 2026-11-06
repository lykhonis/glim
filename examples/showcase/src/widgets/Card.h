#pragma once

#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

#include "Gel.h"
#include "ShowcaseTheme.h"
#include "glim/toolkit.h"

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
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, radius);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    sink.fillGradient(
        box,
        radius,
        themeColor(e, kShowCardTop, Show::kCardTop),
        themeColor(e, kShowCardBottom, Show::kCardBottom)
    );
    const std::uint32_t edge =
        themeColor(e, kShowCardStroke, Show::kCardStroke);
    sink.stroke(box, radius, 1.0f, edge);
    // HUD corner brackets: 4 x L strokes over the hairline.
    const float L = 10.0f;
    auto h = [&](float x, float y, float w) {
      sink.fillRounded({x, y, w, 1.0f}, 1, edge);
    };
    auto v = [&](float x, float y, float hh) {
      sink.fillRounded({x, y, 1.0f, hh}, 1, edge);
    };
    h(box.x, box.y, L);
    v(box.x, box.y, L);
    h(box.x + box.w - L, box.y, L);
    v(box.x + box.w - 1.0f, box.y, L);
    h(box.x, box.y + box.h - 1.0f, L);
    v(box.x, box.y + box.h - L, L);
    h(box.x + box.w - L, box.y + box.h - 1.0f, L);
    v(box.x + box.w - 1.0f, box.y + box.h - L, L);
  }
};

template <class... As>
Block card(As&&... as) {
  Card c;
  c.padH = Show::kPadCard;
  c.padV = Show::kPadCard;
  configure(c, c.children, std::forward<As>(as)...);
  assert(!c.children.empty());
  return finish(std::move(c), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
