#pragma once

// Linear working space: all framebuffer-bound color math blends in
// linear light; sRGB surfaces (swapchain, PNG) convert at the edge.
// Hardware does it on sRGB attachments; this header is the CPU mirror
// (exact sRGB EOTF, matching hardware) plus the pack-side hinge:
// premulLinear() replaces premul() at every render color pack site.
// premul() itself stays sRGB (math tests, non-pixel uses).
#include <glim/math.h>

#include <algorithm>
#include <cmath>

namespace glim {
namespace paint {

inline float srgbToLinear(float c) noexcept {
  return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

inline float linearToSrgb(float c) noexcept {
  const float x = std::clamp(c, 0.0f, 1.0f);
  return x <= 0.0031308f ? x * 12.92f : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}

// Straight sRGB Color → premultiplied LINEAR. Alpha is coverage, never
// converted. Opaque fast path skips the divide (channels are straight).
inline Vec4 premulLinear(const Color& c) noexcept {
  const float a = c.alpha();
  if (a <= 0.0f) {
    return {0.0f, 0.0f, 0.0f, 0.0f};
  }
  return {srgbToLinear(c.red()) * a, srgbToLinear(c.green()) * a, srgbToLinear(c.blue()) * a, a};
}

}  // namespace paint
}  // namespace glim
