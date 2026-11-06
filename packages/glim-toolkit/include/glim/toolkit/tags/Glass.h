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

struct GlassTag {
  bool clear = false;
  int radius = Tokens::kGlassRadius;
  GlassEdge edge = GlassEdge::Full;
  // Plate tint (0xRRGGBBAA). Transparent keeps the white milk wash;
  // a set tint drives the wash toward its color at its alpha.
  std::uint32_t tint = 0;
};

inline GlassTag glass(int radius = Tokens::kGlassRadius) { return {false, radius}; }

inline GlassTag glassClear(int radius = Tokens::kGlassRadius) { return {true, radius}; }

template <>
struct is_tag<GlassTag> : std::true_type {};

template <>
struct is_slot_tag<GlassTag> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
