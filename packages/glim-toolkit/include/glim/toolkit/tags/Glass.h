#pragma once

#include <type_traits>

#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {

// Glass chrome marker: card(glass(), ...) wraps the card box in a
// Regular glass group at paint time. Slot tag — parents never learn
// it, the emitter reads it off the child Block. Nearest wins via env
// if nested. Radius rides the tag so chrome needs no element query.
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
