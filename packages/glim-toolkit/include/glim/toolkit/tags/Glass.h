#pragma once

#include <cstdint>
#include <type_traits>

#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {

// Glass chrome marker; slot tag read by the emitter, nearest wins if nested.
//
// Edge light is a per-side mask: Full is the jeweled plate, None a flat
// frost, Bottom alone a divider under a frosted bar.
enum class GlassEdge : std::uint8_t {
  None = 0,
  Left = 1,
  Right = 2,
  Top = 4,
  Bottom = 8,
  Full = 15,
};

// Lens material preset id. Values pin paint::GlassMaterial (0/1/2);
// the host asserts the shared layout so drift fails the build.
enum class GlassMaterial : std::uint8_t {
  Glass = 0,
  LiquidMetal = 1,
  Water = 2,
};

struct GlassTag {
  bool clear = false;
  int radius = Tokens::kGlassRadius;
  GlassEdge edge = GlassEdge::Full;
  // Plate tint (0xRRGGBBAA). Transparent keeps the white milk wash;
  // a set tint drives the wash toward its color at its alpha.
  // For LiquidMetal this is the metal color (gold, chrome, gunmetal).
  std::uint32_t tint = 0;
  // Lens material preset id (GlassMaterial bits, 0 = refractive glass).
  // No new tag types: metal and water ride the same glass pipeline.
  GlassMaterial material = GlassMaterial::Glass;
};

inline GlassTag glass(GlassTag g = {}) { return g; }

template <>
struct is_tag<GlassTag> : std::true_type {};

template <>
struct is_slot_tag<GlassTag> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
