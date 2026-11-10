#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

// Offsets a node and its children by (dx, dy) logical px, post-layout.
struct TransformTag {
  float dx = 0.f;
  float dy = 0.f;
};

inline TransformTag translate(float dx, float dy) { return {dx, dy}; }

template <>
struct is_tag<TransformTag> : std::true_type {};

template <>
struct is_slot_tag<TransformTag> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
