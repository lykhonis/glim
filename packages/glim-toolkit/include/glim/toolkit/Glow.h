#pragma once

#include <cstdint>

#include "glim/toolkit/Color.h"
#include "glim/toolkit/Layout.h"

namespace glim {
namespace toolkit {

// A glow is additive light: a radial ramp from color at intensity alpha in
// the center to clear at the edge, composited with Plus.
template <class S>
void glow(S& sink, const Box& b, int radius, std::uint32_t color, float intensity = 1.f) {
  sink.pushBlend(BlendMode::Plus);
  sink.fillRadial(b, radius, scaleAlpha(color, intensity), color & 0xffffff00u);
  sink.pop();
}

}  // namespace toolkit
}  // namespace glim
