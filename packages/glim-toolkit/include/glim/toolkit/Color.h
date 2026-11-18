#pragma once

#include <cstdint>

namespace glim {
namespace toolkit {

// Packed RGBA with alpha scaled by a, clamped to 0..1 and rounded, so the
// ends are exact: 1 keeps the color, 0 clears its alpha.
inline std::uint32_t scaleAlpha(std::uint32_t rgba, float a) {
  a = a < 0.f ? 0.f : (a > 1.f ? 1.f : a);
  return (rgba & 0xffffff00u) | (static_cast<unsigned>((rgba & 0xffu) * a + 0.5f) & 0xffu);
}

}  // namespace toolkit
}  // namespace glim
