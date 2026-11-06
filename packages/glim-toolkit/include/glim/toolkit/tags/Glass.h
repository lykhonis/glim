#pragma once

#include <type_traits>

#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {

// Glass chrome marker; slot tag read by the emitter, nearest wins if nested.
struct GlassTag {
  bool clear = false;
  int radius = Tokens::kGlassRadius;
};

inline GlassTag glass(int radius = Tokens::kGlassRadius) { return {false, radius}; }

inline GlassTag glassClear(int radius = Tokens::kGlassRadius) { return {true, radius}; }

template <>
struct is_tag<GlassTag> : std::true_type {};

template <>
struct is_slot_tag<GlassTag> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
