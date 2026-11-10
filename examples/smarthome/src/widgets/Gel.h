#pragma once

#include <cstdint>
#include <string>

#include "HomeTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

// Alpha-scale a packed RGBA color. Endpoints exact; mid-flight rounding
// (±1) never touches a rest state — callers skip the layer at 0.
inline std::uint32_t withAlpha(std::uint32_t rgba, float a) {
  if (a >= 1.0f) return rgba;
  if (a <= 0.0f) return rgba & 0xffffff00u;
  const unsigned alpha =
      static_cast<unsigned>((rgba & 0xffu) * a + 0.5f);
  return (rgba & 0xffffff00u) | (alpha & 0xffu);
}

// Aqua gel coat, composited ADDITIVE (Plus): speculars add light
// instead of washing the base toward gray like SrcOver white does.
// Faces ≤6px stay flat.
inline void gelCoat(
    AnySink& sink, const Box& box, int radius, float alpha = 1.0f
) {
  if (box.h <= 6.0f || box.w <= 6.0f) {
    return;
  }
  sink.pushBlend(BlendMode::Plus);
  const int inner = radius > 3 ? radius - 2 : 1;
  Box dome{box.x + 1.0f, box.y + 1.0f, box.w - 2.0f, box.h * 0.48f};
  sink.fillRounded(dome, inner, withAlpha(Show::kGelGloss, alpha));
  // The 1px light only on tall faces (>12px); on thin fills it reads as
  // wire.
  const float inset = static_cast<float>(radius) * 0.6f + 2.0f;
  if (box.h > 12.0f && box.w > inset * 2.0f + 4.0f) {
    Box light{box.x + inset, box.y + 1.0f, box.w - inset * 2.0f, 1.0f};
    sink.fillRounded(light, 1, withAlpha(Show::kGelEdge, alpha));
  }
  sink.pop();
}

}  // namespace toolkit
}  // namespace glim
