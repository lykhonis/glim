#pragma once

// SidePanel: fixed-width rail shell. Owns the full-height layout
// (top drag clearance + bottom-anchored status); children come from
// the caller. No animation state: the rail no longer collapses.

#include <cstddef>
#include <utility>
#include <vector>

#include "ShowcaseTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct SidePanel {
  static constexpr const char* kKind = "Side";
  std::vector<Block> children;
  float width = 232.0f;
  Box place(Constraints c, const Engine& e) const {
    float w = width;
    if (w < 8.0f) w = 8.0f;
    float h = 200.0f;
    if (!isUnbounded(c.maxH)) h = c.maxH;
    // Custom titlebar reserves the top 48px as the drag strip under
    // the traffic lights; content starts below it.
    Constraints ic{
        w - 32.0f < 8.0f ? 8.0f : w - 32.0f,
        w - 32.0f,
        0,
        h > 64.0f ? h - 64.0f : 0
    };
    detail::Flow f = detail::flowColumn(children, 8, ic, e);
    float want = f.size.h + 64.0f;
    if (want > h) h = want;
    return detail::resolveSize(w, h, c);
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    float w = box.w - 32.0f;
    if (w < 8.0f) w = 8.0f;
    detail::Flow f = detail::flowColumn(
        children, 8, {0, w, 0, box.h > 64.0f ? box.h - 64.0f : 0}, e
    );
    Box b = f.children[i];
    b.x += box.x + 16.0f;
    b.y += box.y + 48.0f;
    return b;
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 0);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    sink.fillGradient(
        box,
        0,
        themeColor(e, kShowCardTop, Show::kCardTop),
        themeColor(e, kShowCardBottom, Show::kCardBottom)
    );
    Box edge{box.x + box.w - 1.0f, box.y, 1.0f, box.h};
    sink.fillRounded(
        edge, 1, themeColor(e, kShowCardStroke, Show::kCardStroke)
    );
  }
};

template <class... As>
auto sidePanel(SidePanel p, As&&... as) {
  configure(p, p.children, std::forward<As>(as)...);
  return finish(std::move(p), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
