#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

// Glass chrome marker: card(glassClear(), ...) wraps the card box in a
// Clear glass group at paint time. Plain glass() is Regular. Slot tag —
// parents never learn it, the paint bridge reads has<GlassTag>() off the
// child Block. Nearest wins via env if nested.
struct GlassTag {
  bool clear = false;
};

inline GlassTag glass() { return {false}; }

inline GlassTag glassClear() { return {true}; }

template <>
struct is_tag<GlassTag> : std::true_type {};

template <>
struct is_slot_tag<GlassTag> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
