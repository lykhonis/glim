#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

// Rotates a node and its children post-layout: degrees clockwise on
// screen about (cx, cy) logical px.
struct RotateTag {
  float angleDeg = 0.f;
  float cx = 0.f;
  float cy = 0.f;
};

inline RotateTag rotate(float angleDeg, float cx, float cy) { return {angleDeg, cx, cy}; }

template <>
struct is_tag<RotateTag> : std::true_type {};

template <>
struct is_slot_tag<RotateTag> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
