#pragma once

#include <glim/math.h>

namespace glim::shell {

// A projected seat's layer transform: Glim's world transform re-expressed
// about the seat rect's own origin, where a host layer anchors (K88).
inline Mat4 slotLayerTransform(Rect local, const Mat4& world) {
  return Mat4::translate(-local.origin.x, -local.origin.y) * world *
         Mat4::translate(local.origin.x, local.origin.y);
}

// A copy's transform in the view's layer space: where the copy draws the
// view's content, given the view's own placement.
inline Mat4 slotCopyTransform(Rect local, const Mat4& world, const PlacedRect& copy) {
  const Mat4 back = world.inverse().value_or(Mat4::identity());
  return Mat4::translate(-local.origin.x, -local.origin.y) * back * copy.transform *
         Mat4::translate(copy.local.origin.x, copy.local.origin.y);
}

// The unit-square line a linear gradient runs along for ramp: fromAlpha at
// start, toAlpha at end.
struct RampLine {
  Vec2 start{};
  Vec2 end{};
};
inline RampLine rampLine(const AlphaRamp& ramp) {
  const float len2 = ramp.step.x * ramp.step.x + ramp.step.y * ramp.step.y;
  if (len2 <= 1e-12f) return {ramp.origin, {ramp.origin.x, ramp.origin.y + 1}};
  return {ramp.origin, {ramp.origin.x + ramp.step.x / len2, ramp.origin.y + ramp.step.y / len2}};
}

// Core Animation's CATransform3D stores the same column-major 16 values.
template <class CATransform>
CATransform toCoreAnimation(const Mat4& m) {
  CATransform t;
  auto* out = &t.m11;
  for (int i = 0; i < 16; ++i) out[i] = m.m[i];
  return t;
}

}  // namespace glim::shell
