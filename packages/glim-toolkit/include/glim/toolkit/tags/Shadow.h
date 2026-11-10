#pragma once

#include <cstdint>
#include <type_traits>

namespace glim {
namespace toolkit {

// Drop shadow cast by a node and its children. `opacity` fades the caster
// only: at 0 the node still casts but paints nothing itself.
struct ShadowTag {
  float sigma = 16.f;
  float offsetX = 0.f;
  float offsetY = 4.f;
  std::uint32_t color = 0x000000a6u;
  float expandPx = 0.f;
  float opacity = 1.f;
};

inline ShadowTag shadow(
    float sigma = 16.f,
    float offsetX = 0.f,
    float offsetY = 4.f,
    std::uint32_t color = 0x000000a6u,
    float expandPx = 0.f,
    float opacity = 1.f
) {
  return {sigma, offsetX, offsetY, color, expandPx, opacity};
}

template <>
struct is_tag<ShadowTag> : std::true_type {};

template <>
struct is_slot_tag<ShadowTag> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
