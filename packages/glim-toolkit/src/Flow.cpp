#include <algorithm>
#include <vector>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/Toolkit.h"

namespace glim {
namespace toolkit {
namespace detail {

namespace {

Constraints childConstraints(Constraints c) { return {0, c.maxW, 0, c.maxH}; }

Flow flowAxis(
    const std::vector<Block>& children, int gap, Constraints c, const Engine& e, bool horizontal
) {
  Flow f;
  std::size_t nFlex = 0;
  for (const auto& child : children)
    if (child.has<ExpandTag>()) ++nFlex;
  Constraints cc = childConstraints(c);
  const float maxMain = horizontal ? c.maxW : c.maxH;
  const float minMain = horizontal ? c.minW : c.minH;
  const float maxCross = horizontal ? c.maxH : c.maxW;
  const float minCross = horizontal ? c.minH : c.minW;
  auto placeMain = [&](const Block& child, float share, bool fixed) -> Box {
    if (!fixed) return e.place(child, cc);
    return horizontal ? e.place(child, {share, share, 0, c.maxH})
                      : e.place(child, {0, c.maxW, share, share});
  };
  if (nFlex == 0) {
    float main = 0, cross = 0;
    for (const auto& child : children) {
      Box b = e.place(child, cc);
      if (horizontal) {
        b.x = main;
        b.y = 0;
        main += b.w + static_cast<float>(gap);
        cross = std::max(cross, b.h);
      } else {
        b.x = 0;
        b.y = main;
        main += b.h + static_cast<float>(gap);
        cross = std::max(cross, b.w);
      }
      f.children.push_back(b);
    }
    if (!children.empty()) main -= static_cast<float>(gap);
    const float w = horizontal ? clampF(main, minMain, maxMain) : clampF(cross, minCross, maxCross);
    const float h = horizontal ? clampF(cross, minCross, maxCross) : clampF(main, minMain, maxMain);
    f.size = {0, 0, w, h};
    return f;
  }
  // Measure rigid children first; flexible children split the remainder equally.
  std::vector<Box> rigid(children.size());
  std::vector<bool> rigidDone(children.size(), false);
  float rigidMain = 0;
  for (std::size_t i = 0; i < children.size(); ++i) {
    if (children[i].has<ExpandTag>()) continue;
    Box b = e.place(children[i], cc);
    rigid[i] = b;
    rigidDone[i] = true;
    rigidMain += horizontal ? b.w : b.h;
  }
  float gaps = children.size() > 1 ? static_cast<float>(gap) * (children.size() - 1) : 0.0f;
  bool bounded = !isUnbounded(maxMain);
  float remainder = bounded ? maxMain - rigidMain - gaps : -1.0f;
  if (!bounded || remainder < 0) {
    // Unbounded axis or rigid overflow: flexible children take natural size and spill.
    float main = 0, cross = 0;
    for (const auto& child : children) {
      Box b = e.place(child, cc);
      if (horizontal) {
        b.x = main;
        b.y = 0;
        main += b.w + static_cast<float>(gap);
        cross = std::max(cross, b.h);
      } else {
        b.x = 0;
        b.y = main;
        main += b.h + static_cast<float>(gap);
        cross = std::max(cross, b.w);
      }
      f.children.push_back(b);
    }
    if (!children.empty()) main -= static_cast<float>(gap);
    const float w = horizontal ? clampF(main, minMain, maxMain) : clampF(cross, minCross, maxCross);
    const float h = horizontal ? clampF(cross, minCross, maxCross) : clampF(main, minMain, maxMain);
    f.size = {0, 0, w, h};
    return f;
  }
  float share = remainder / static_cast<float>(nFlex);
  float main = 0, cross = 0;
  for (std::size_t i = 0; i < children.size(); ++i) {
    Box b;
    if (rigidDone[i]) {
      b = rigid[i];
    } else {
      Box inner = placeMain(children[i], share, true);
      b = horizontal ? Box{0, 0, share, inner.h} : Box{0, 0, inner.w, share};
    }
    if (horizontal) {
      b.x = main;
      b.y = 0;
      main += b.w + static_cast<float>(gap);
      cross = std::max(cross, b.h);
    } else {
      b.x = 0;
      b.y = main;
      main += b.h + static_cast<float>(gap);
      cross = std::max(cross, b.w);
    }
    f.children.push_back(b);
  }
  if (!children.empty()) main -= static_cast<float>(gap);
  const float w = horizontal ? clampF(main, minMain, maxMain) : clampF(cross, minCross, maxCross);
  const float h = horizontal ? clampF(cross, minCross, maxCross) : clampF(main, minMain, maxMain);
  f.size = {0, 0, w, h};
  return f;
}

}  // namespace

Flow flowRow(const std::vector<Block>& children, int gap, Constraints c, const Engine& e) {
  return flowAxis(children, gap, c, e, true);
}

Flow flowColumn(const std::vector<Block>& children, int gap, Constraints c, const Engine& e) {
  return flowAxis(children, gap, c, e, false);
}

Flow flowStack(const std::vector<Block>& children, Constraints c, const Engine& e) {
  Flow f;
  float w = 0, h = 0;
  for (const auto& child : children) {
    Box b = e.place(child, c);
    b.x = 0;
    b.y = 0;
    f.children.push_back(b);
    w = std::max(w, b.w);
    h = std::max(h, b.h);
  }
  f.size = {0, 0, clampF(w, c.minW, c.maxW), clampF(h, c.minH, c.maxH)};
  return f;
}

}  // namespace detail
}  // namespace toolkit
}  // namespace glim
