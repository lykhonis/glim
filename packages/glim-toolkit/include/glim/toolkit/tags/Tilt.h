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
  bool active() const { return x != 0.f || y != 0.f; }
};

inline TiltTag tilt(TiltTag t) { return t; }

// The tilt as a paint transform about (cx, cy); paint and hit testing share it.
inline Mat4 tiltMatrix(float xDeg, float yDeg, float perspective, float cx, float cy) {
  constexpr float kRad = 3.14159265f / 180.0f;
  return Mat4::translate(cx, cy) * Mat4::perspective(perspective) * Mat4::rotateX(xDeg * kRad) *
         Mat4::rotateY(yDeg * kRad) * Mat4::translate(-cx, -cy);
}

inline Mat4 tiltMatrix(const TiltTag& t, const Box& box) {
  return tiltMatrix(t.x, t.y, t.perspective, box.x + box.w / 2, box.y + box.h / 2);
}

template <>
struct is_tag<TiltTag> : std::true_type {};

template <>
struct is_slot_tag<TiltTag> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
