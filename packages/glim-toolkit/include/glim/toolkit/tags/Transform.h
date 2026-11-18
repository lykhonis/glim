#pragma once

#include <type_traits>

#include "glim/math.h"
#include "glim/toolkit/Layout.h"

namespace glim {
namespace toolkit {

// Moves, turns, scales, skews or tilts a node and its children after
// layout, about anchor (a share of the node's box, center by default).
// Parts compose in a fixed order, innermost first: scale, skew, rotate,
// tilt (seen from perspective px in front), then translate. Paint and hit
// testing share transformMatrix, so pointers hit what is drawn.
struct TransformTag {
  Vec2 translate{};      // logical px
  float rotate = 0.f;    // degrees clockwise on screen
  Vec2 scale{1.f, 1.f};  // negative flips
  Vec2 skew{};           // degrees; x leans vertical edges, y horizontal ones
  Vec2 tilt{};           // degrees; x tips the bottom away, y the right edge
  float perspective = 800.f;
  Vec2 anchor{0.5f, 0.5f};
  // Shrinks the content about anchor until its projection stays inside the
  // node's box, so a tilted near edge never covers neighbors.
  bool fit = false;
};

inline TransformTag transform(TransformTag t) { return t; }
inline TransformTag translate(float dx, float dy) { return {.translate = {dx, dy}}; }
// The tag's matrix about pivot in logical px, ignoring anchor; fit scales
// the content. Custom elements push it for transforms inside their paint.
inline Mat4 transformAt(const TransformTag& t, Vec2 pivot, float fit = 1.0f) {
  constexpr float kRad = 3.14159265f / 180.0f;
  Mat4 m = Mat4::translate(t.translate.x + pivot.x, t.translate.y + pivot.y);
  if (t.tilt.x != 0.f || t.tilt.y != 0.f) {
    m = m * Mat4::perspective(t.perspective) * Mat4::rotateX(t.tilt.x * kRad) *
        Mat4::rotateY(t.tilt.y * kRad);
  }
  if (t.rotate != 0.f) m = m * Mat4::rotateZ(t.rotate * kRad);
  if (t.skew.x != 0.f || t.skew.y != 0.f) m = m * Mat4::skew(t.skew.x * kRad, t.skew.y * kRad);
  return m * Mat4::scale(t.scale.x * fit, t.scale.y * fit) * Mat4::translate(-pivot.x, -pivot.y);
}

namespace detail {

// 1, or for fit the largest scale whose projection stays inside box.
// Projection is not linear in scale, so it halves toward it; a dozen steps
// is well under a pixel.
inline float fitScale(const TransformTag& t, const Box& box, Vec2 pivot) {
  if (!t.fit) return 1.0f;
  const auto fits = [&](float s) {
    const Mat4 m = transformAt(t, pivot, s);
    for (const Vec2 c :
         {Vec2{box.x, box.y},
          Vec2{box.x + box.w, box.y},
          Vec2{box.x, box.y + box.h},
          Vec2{box.x + box.w, box.y + box.h}}) {
      const Vec3 q = projectPlane(m, c);
      if (q.z <= 0) return false;
      const float px = q.x / q.z, py = q.y / q.z;
      if (px < box.x - 0.25f || px > box.x + box.w + 0.25f || py < box.y - 0.25f ||
          py > box.y + box.h + 0.25f)
        return false;
    }
    return true;
  };
  if (fits(1.0f)) return 1.0f;
  float lo = 0.0f, hi = 1.0f;
  for (int i = 0; i < 12; ++i) {
    const float mid = (lo + hi) / 2;
    (fits(mid) ? lo : hi) = mid;
  }
  return lo;
}

}  // namespace detail

inline Mat4 transformMatrix(const TransformTag& t, const Box& box) {
  const Vec2 pivot{box.x + box.w * t.anchor.x, box.y + box.h * t.anchor.y};
  return transformAt(t, pivot, detail::fitScale(t, box, pivot));
}

template <>
struct is_tag<TransformTag> : std::true_type {};
template <>
struct is_slot_tag<TransformTag> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
