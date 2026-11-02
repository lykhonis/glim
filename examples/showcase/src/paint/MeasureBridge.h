#pragma once

// Real text metrics for layout. Context::measureText sums atlas
// advances with the renderer's exact skip rule, so laid-out boxes
// match pens pixel-for-pixel. Published into the Engine env —
// detail::measureText uses it when present, fixed advance otherwise
// (headless toolkit_test keeps the deterministic fallback).
#include <glim/paint/Context.h>

#include <typeinfo>

#include "glim/toolkit/Measure.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {
namespace paint {

inline float measureBridge(const char* text, float sizePx) {
  static glim::paint::Context measurer;
  return measurer.measureText(text, sizePx).width;
}

inline Engine measureEngine() {
  static Measurer meas{&measureBridge};
  static Env node{&meas, &typeid(Measurer), nullptr};
  return Engine{&node};
}

}  // namespace paint
}  // namespace toolkit
}  // namespace glim
