#pragma once

#include <glim/paint/Scene.h>

namespace glim::paint::raster {

struct Pixel {
  float r, g, b, a;
};

// Rec.709 luma of linear RGB.
inline float luma(const Pixel& p) { return p.r * 0.2126f + p.g * 0.7152f + p.b * 0.0722f; }

inline void srcOver(Pixel& dst, Pixel src) {
  const float ia = 1.0f - src.a;
  dst.r = src.r + dst.r * ia;
  dst.g = src.g + dst.g * ia;
  dst.b = src.b + dst.b * ia;
  dst.a = src.a + dst.a * ia;
}

inline void plus(Pixel& dst, Pixel src) {
  dst.r += src.r;
  dst.g += src.g;
  dst.b += src.b;
  dst.a += src.a;
}

inline void blendPixel(Pixel& dst, Pixel src, Blend blend) {
  if (blend == Blend::Plus) {
    plus(dst, src);
  } else {
    srcOver(dst, src);
  }
}

}  // namespace glim::paint::raster
