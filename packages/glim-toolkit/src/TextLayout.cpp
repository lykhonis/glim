#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {
namespace detail {

float clampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

Box resolveSize(float naturalW, float naturalH, Constraints c) {
  float w = (c.minW == c.maxW) ? c.maxW : (naturalW < c.minW ? c.minW : naturalW);
  float h = (c.minH == c.maxH) ? c.maxH : (naturalH < c.minH ? c.minH : naturalH);
  return {0, 0, w, h};
}

Constraints inset(Constraints c, float dx, float dy) {
  if (!isUnbounded(c.maxW)) {
    c.maxW -= dx;
    if (c.maxW < 0) c.maxW = 0;
  }
  if (!isUnbounded(c.maxH)) {
    c.maxH -= dy;
    if (c.maxH < 0) c.maxH = 0;
  }
  if (c.minW > 0) {
    c.minW -= dx;
    if (c.minW < 0) c.minW = 0;
  }
  if (c.minH > 0) {
    c.minH -= dy;
    if (c.minH < 0) c.minH = 0;
  }
  if (c.minW > c.maxW) c.minW = c.maxW;
  if (c.minH > c.maxH) c.minH = c.maxH;
  return c;
}

}  // namespace detail
}  // namespace toolkit
}  // namespace glim
