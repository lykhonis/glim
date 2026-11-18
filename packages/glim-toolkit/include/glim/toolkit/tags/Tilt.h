#pragma once

#include <type_traits>

#include "glim/math.h"
#include "glim/toolkit/Layout.h"

namespace glim {
namespace toolkit {

// Tilts a node and its children in 3D about the node's center, post-layout:
// x tips the bottom edge away, y swings the right edge away (degrees), seen
// from perspective px in front. Pointers hit what is drawn.
struct TiltTag {
  float x = 0.f;
  float y = 0.f;
  float perspective = 800.f;
  // Shrinks the turned content about its center until its projection stays
  // inside the node's box, so the near edge never covers neighbors.
  bool fit = false;
  bool active() const { return x != 0.f || y != 0.f; }
};

inline TiltTag tilt(TiltTag t) { return t; }

// The tilt as a paint transform about (cx, cy), the content first scaled by
// scale about it; paint and hit testing share it.
inline Mat4 tiltMatrix(
  float xDeg, float yDeg, float perspective, float cx, float cy, float scale = 1.0f
) {
  constexpr float kRad = 3.14159265f / 180.0f;
  return Mat4::translate(cx, cy) * Mat4::perspective(perspective) * Mat4::rotateX(xDeg * kRad) *
         Mat4::rotateY(yDeg * kRad) * Mat4::scale(scale, scale) * Mat4::translate(-cx, -cy);
}

// 1, or for fit the largest scale whose projection stays inside box.
// Projection is not linear in scale, so it halves toward it; a dozen steps
// is well under a pixel.
inline float tiltScale(const TiltTag& t, const Box& box) {
  if (!t.fit) return 1.0f;
  const float cx = box.x + box.w / 2, cy = box.y + box.h / 2;
  const auto fits = [&](float s) {
    const Mat4 m = tiltMatrix(t.x, t.y, t.perspective, cx, cy, s);
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

inline Mat4 tiltMatrix(const TiltTag& t, const Box& box) {
  return tiltMatrix(
    t.x, t.y, t.perspective, box.x + box.w / 2, box.y + box.h / 2, tiltScale(t, box)
  );
}

template <>
struct is_tag<TiltTag> : std::true_type {};

template <>
struct is_slot_tag<TiltTag> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
