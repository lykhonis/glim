#pragma once

// Grid: as many equal columns as fit `minCell`, rows as tall as their
// tallest cell. Resizing the window reflows the cards.

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Grid : WithGap {
  static constexpr const char* kKind = "Grid";
  const char* kind() const { return kKind; }
  std::vector<Block> children;
  float minCell = 260.0f;
  // 0 means as many columns as fit. A cap keeps wide windows on large
  // cells instead of ever-thinner ones.
  int maxCols = 0;

  struct Cells {
    Box size;
    std::vector<Box> boxes;
  };

  Cells layout(float width, const Engine& e) const {
    const float g = static_cast<float>(gap);
    const std::size_t n = children.size();
    std::size_t cols = n;
    if (!isUnbounded(width))
      cols = static_cast<std::size_t>(
          std::max(1.0f, (width + g) / (minCell + g))
      );
    cols = std::max<std::size_t>(1, std::min(cols, n));
    if (maxCols > 0)
      cols = std::min(cols, static_cast<std::size_t>(maxCols));
    const float cell = isUnbounded(width)
                           ? minCell
                           : (width - g * static_cast<float>(cols - 1)
                             ) / static_cast<float>(cols);
    Cells out;
    out.boxes.resize(n);
    float y = 0.0f;
    for (std::size_t r = 0; r < n; r += cols) {
      float rowH = 0.0f;
      for (std::size_t i = r; i < std::min(n, r + cols); ++i) {
        Box b = e.place(children[i], {cell, cell, 0, kUnbounded});
        b.x = static_cast<float>(i - r) * (cell + g);
        b.y = y;
        b.w = cell;
        out.boxes[i] = b;
        rowH = std::max(rowH, b.h);
      }
      // Cells in a row share the row height, so card bottoms line up.
      for (std::size_t i = r; i < std::min(n, r + cols); ++i)
        out.boxes[i].h = rowH;
      y += rowH + g;
    }
    const float w = cell * static_cast<float>(cols) +
                    g * static_cast<float>(cols - 1);
    out.size = {0, 0, w, n == 0 ? 0.0f : y - g};
    return out;
  }
  Box place(Constraints c, const Engine& e) const {
    Box s = layout(c.maxW, e).size;
    return detail::resolveSize(s.w, s.h, c);
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    Box b = layout(box.w, e).boxes[i];
    b.x += box.x;
    b.y += box.y;
    return b;
  }
};

template <class... As>
auto grid(Grid g, As&&... as) {
  configure(g, g.children, std::forward<As>(as)...);
  return finish(std::move(g), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
