#include <algorithm>
#include <vector>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/Toolkit.h"

namespace glim {
namespace toolkit {
namespace detail {

namespace {

Constraints childConstraints(Constraints c) { return {0, c.maxW, 0, c.maxH}; }

}  // namespace

Flow flowRow(const std::vector<Block>& children, int gap, Constraints c, const Engine& e) {
  Flow f;
  std::size_t nFlex = 0;
  for (const auto& child : children)
    if (child.has<ExpandTag>()) ++nFlex;
  Constraints cc = childConstraints(c);
  if (nFlex == 0) {
    float x = 0, h = 0;
    for (const auto& child : children) {
      Box b = e.place(child, cc);
      b.x = x;
      b.y = 0;
      f.children.push_back(b);
      x += b.w + static_cast<float>(gap);
      h = std::max(h, b.h);
    }
    if (!children.empty()) x -= static_cast<float>(gap);
    f.size = {0, 0, clampF(x, c.minW, c.maxW), clampF(h, c.minH, c.maxH)};
    return f;
  }
  // Measure rigid children first; flexible children split the remainder equally.
  std::vector<Box> rigid(children.size());
  std::vector<bool> rigidDone(children.size(), false);
  float rigidW = 0;
  for (std::size_t i = 0; i < children.size(); ++i) {
    if (children[i].has<ExpandTag>()) continue;
    Box b = e.place(children[i], cc);
    rigid[i] = b;
    rigidDone[i] = true;
    rigidW += b.w;
  }
  float gaps = children.size() > 1 ? static_cast<float>(gap) * (children.size() - 1) : 0.0f;
  bool bounded = !isUnbounded(c.maxW);
  float remainder = bounded ? c.maxW - rigidW - gaps : -1.0f;
  if (!bounded || remainder < 0) {
    // Unbounded axis or rigid overflow: flexible children take natural size and spill.
    float x = 0, h = 0;
    for (std::size_t i = 0; i < children.size(); ++i) {
      Box b = e.place(children[i], cc);
      b.x = x;
      b.y = 0;
      f.children.push_back(b);
      x += b.w + static_cast<float>(gap);
      h = std::max(h, b.h);
    }
    if (!children.empty()) x -= static_cast<float>(gap);
    f.size = {0, 0, clampF(x, c.minW, c.maxW), clampF(h, c.minH, c.maxH)};
    return f;
  }
  float share = remainder / static_cast<float>(nFlex);
  float x = 0, h = 0;
  for (std::size_t i = 0; i < children.size(); ++i) {
    Box b;
    if (rigidDone[i]) {
      b = rigid[i];
    } else {
      Box inner = e.place(children[i], {share, share, 0, c.maxH});
      b = {0, 0, share, inner.h};
    }
    b.x = x;
    b.y = 0;
    f.children.push_back(b);
    x += b.w + static_cast<float>(gap);
    h = std::max(h, b.h);
  }
  if (!children.empty()) x -= static_cast<float>(gap);
  f.size = {0, 0, clampF(x, c.minW, c.maxW), clampF(h, c.minH, c.maxH)};
  return f;
}

Flow flowColumn(const std::vector<Block>& children, int gap, Constraints c, const Engine& e) {
  Flow f;
  std::size_t nFlex = 0;
  for (const auto& child : children)
    if (child.has<ExpandTag>()) ++nFlex;
  Constraints cc = childConstraints(c);
  if (nFlex == 0) {
    float y = 0, w = 0;
    for (const auto& child : children) {
      Box b = e.place(child, cc);
      b.x = 0;
      b.y = y;
      f.children.push_back(b);
      y += b.h + static_cast<float>(gap);
      w = std::max(w, b.w);
    }
    if (!children.empty()) y -= static_cast<float>(gap);
    f.size = {0, 0, clampF(w, c.minW, c.maxW), clampF(y, c.minH, c.maxH)};
    return f;
  }
  std::vector<Box> rigid(children.size());
  std::vector<bool> rigidDone(children.size(), false);
  float rigidH = 0;
  for (std::size_t i = 0; i < children.size(); ++i) {
    if (children[i].has<ExpandTag>()) continue;
    Box b = e.place(children[i], cc);
    rigid[i] = b;
    rigidDone[i] = true;
    rigidH += b.h;
  }
  float gaps = children.size() > 1 ? static_cast<float>(gap) * (children.size() - 1) : 0.0f;
  bool bounded = !isUnbounded(c.maxH);
  float remainder = bounded ? c.maxH - rigidH - gaps : -1.0f;
  if (!bounded || remainder < 0) {
    float y = 0, w = 0;
    for (std::size_t i = 0; i < children.size(); ++i) {
      Box b = e.place(children[i], cc);
      b.x = 0;
      b.y = y;
      f.children.push_back(b);
      y += b.h + static_cast<float>(gap);
      w = std::max(w, b.w);
    }
    if (!children.empty()) y -= static_cast<float>(gap);
    f.size = {0, 0, clampF(w, c.minW, c.maxW), clampF(y, c.minH, c.maxH)};
    return f;
  }
  float share = remainder / static_cast<float>(nFlex);
  float y = 0, w = 0;
  for (std::size_t i = 0; i < children.size(); ++i) {
    Box b;
    if (rigidDone[i]) {
      b = rigid[i];
    } else {
      Box inner = e.place(children[i], {0, c.maxW, share, share});
      b = {0, 0, inner.w, share};
    }
    b.x = 0;
    b.y = y;
    f.children.push_back(b);
    y += b.h + static_cast<float>(gap);
    w = std::max(w, b.w);
  }
  if (!children.empty()) y -= static_cast<float>(gap);
  f.size = {0, 0, clampF(w, c.minW, c.maxW), clampF(y, c.minH, c.maxH)};
  return f;
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
