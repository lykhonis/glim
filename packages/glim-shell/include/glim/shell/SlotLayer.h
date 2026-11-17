#pragma once

#include <glim/math.h>

namespace glim::shell {

// A projected seat's layer transform: Glim's world transform re-expressed
// about the seat rect's own origin, where a host layer anchors (K88).
inline Mat4 slotLayerTransform(Rect local, const Mat4& world) {
  return Mat4::translate(-local.origin.x, -local.origin.y) * world *
         Mat4::translate(local.origin.x, local.origin.y);
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
