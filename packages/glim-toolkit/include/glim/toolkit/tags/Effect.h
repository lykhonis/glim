#pragma once

#include <cstdint>
#include <type_traits>

namespace glim {
namespace toolkit {

// Effect scope around a node: the host lowers each kind to its own
// primitives. Glow is an additive radial light, opaque at the center.
enum class EffectKind : std::uint8_t {
  Glow = 0,
};

struct EffectTag {
  EffectKind kind = EffectKind::Glow;
  int radius = 0;
  std::uint32_t color = 0xff6a00ffu;
  // 0..1 multiplier on the color alpha at the center.
  float intensity = 1.f;
};

inline EffectTag effectGlow(EffectTag e = {}) {
  e.kind = EffectKind::Glow;
  return e;
}

template <>
struct is_tag<EffectTag> : std::true_type {};

template <>
struct is_slot_tag<EffectTag> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
