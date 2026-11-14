#pragma once

#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

#include "Gel.h"
#include "HomeTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Card : WithGap, WithRadius, WithThrough, WithPad, WithSemantic {
  static constexpr const char* kKind = "Card";
  std::vector<Block> children;
  // Glass cards paint nothing: the native plate is the whole surface.
  bool frosted = false;
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
    if (frosted) return;
    sink.fillGradient(
        box,
        radius,
        themeColor(e, kShowCardTop, Show::kCardTop),
        themeColor(e, kShowCardBottom, Show::kCardBottom)
    );
    sink.stroke(
        box,
        radius,
        1.0f,
        themeColor(e, kShowCardStroke, Show::kCardStroke)
    );
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

// Pane: one paint group around one child. Sibling glass plates in
// separate groups never fuse into one field, and each card's content
// stays ordered after its own plate. The clip sits a hairline outside
// the child so its edge stroke survives.
struct Pane {
  static constexpr const char* kKind = "Pane";
  static constexpr bool kClipsChildren = true;
  static constexpr float kBleed = 1.0f;
  std::vector<Block> children;
  Box place(Constraints c, const Engine& e) const {
    const Constraints ic = detail::inset(c, 2 * kBleed, 2 * kBleed);
    Box b = e.place(children[0], ic);
    return {0, 0, b.w + 2 * kBleed, b.h + 2 * kBleed};
  }
  Box childBox(std::size_t, const Box& box, const Engine& e) const {
    // Fill the box: a grid row stretches its panes, so plates in one
    // row end on one line.
    const float h = box.h - 2 * kBleed;
    Box b = e.place(
        children[0], {box.w - 2 * kBleed, box.w - 2 * kBleed, h, h}
    );
    b.x = box.x + kBleed;
    b.y = box.y + kBleed;
    return b;
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 0);
  }
  void paint(const Box&, AnySink&, const Engine&) const {}
};

// Card on a frosted glass plate of the same radius; `tint` washes the
// plate (0xRRGGBBAA). Each card is its own pane: a flat plate that
// never melts into its neighbors, edged by the glass itself.
template <class... As>
Block glassCard(std::uint32_t tint, As&&... as) {
  Card c;
  c.padH = Show::kPadCard;
  c.padV = Show::kPadCard;
  c.frosted = true;
  configure(c, c.children, std::forward<As>(as)...);
  assert(!c.children.empty());
  GlassTag plate = glass({.radius = c.radius});
  plate.tint = tint;
  Pane pane;
  pane.children.push_back(
      finish(std::move(c), std::forward<As>(as)..., plate)
  );
  return finish(std::move(pane));
}

}  // namespace toolkit
}  // namespace glim
