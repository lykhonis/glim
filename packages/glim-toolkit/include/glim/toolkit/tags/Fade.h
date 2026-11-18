#pragma once

#include <cstdint>
#include <type_traits>

#include "glim/math.h"
#include "glim/toolkit/Layout.h"

namespace glim {
namespace toolkit {

enum class FadeEdge : std::uint8_t { Top, Bottom, Left, Right };

// Ramps the alpha of a node's children from fromAlpha at edge to toAlpha
// length px inward (0 = across the node), held beyond. It applies in the
// node's own space, inside its transform.
struct FadeTag {
  FadeEdge edge = FadeEdge::Bottom;
  float length = 0.f;
  float fromAlpha = 0.f;
  float toAlpha = 1.f;
};

inline FadeTag fade(FadeTag f) { return f; }

inline Fade fadeIn(const FadeTag& f, const Box& b) {
  const bool vertical = f.edge == FadeEdge::Top || f.edge == FadeEdge::Bottom;
  const float span = f.length > 0.f ? f.length : (vertical ? b.h : b.w);
  const float cx = b.x + b.w / 2, cy = b.y + b.h / 2;
  switch (f.edge) {
    case FadeEdge::Top:
      return {{cx, b.y}, {cx, b.y + span}, f.fromAlpha, f.toAlpha};
    case FadeEdge::Bottom:
      return {{cx, b.y + b.h}, {cx, b.y + b.h - span}, f.fromAlpha, f.toAlpha};
    case FadeEdge::Left:
      return {{b.x, cy}, {b.x + span, cy}, f.fromAlpha, f.toAlpha};
    case FadeEdge::Right:
      return {{b.x + b.w, cy}, {b.x + b.w - span, cy}, f.fromAlpha, f.toAlpha};
  }
  return {};
}

template <>
struct is_tag<FadeTag> : std::true_type {};
template <>
struct is_slot_tag<FadeTag> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
