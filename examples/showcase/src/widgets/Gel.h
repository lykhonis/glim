#pragma once

#include <cstdint>
#include <string>

#include "ShowcaseTheme.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {

// Aqua gel coat: specular dome over the top half plus a 1px inner top
// light, composited ADDITIVE (Plus): speculars add light instead of
// washing the saturated base toward gray like SrcOver white does.
// Paint after the base gradient, pop before the rim stroke. One home
// for the gel rule; widgets supply their own box and radius.
// Faces shorter than 7px are too thin to dome — they stay flat.
inline void gelCoat(AnySink& sink, const Box& box, int radius) {
  if (box.h <= 6.0f || box.w <= 6.0f) {
    return;
  }
  sink.pushBlend(BlendMode::Plus);
  const int inner = radius > 3 ? radius - 2 : 1;
  Box dome{box.x + 1.0f, box.y + 1.0f, box.w - 2.0f, box.h * 0.48f};
  sink.fillRounded(dome, inner, Show::kGelGloss);
  // The 1px light only on tall faces: on thin fills (slider,
  // progress) it reads as a white wire instead of a highlight.
  const float inset = static_cast<float>(radius) * 0.6f + 2.0f;
  if (box.h > 12.0f && box.w > inset * 2.0f + 4.0f) {
    Box light{box.x + inset, box.y + 1.0f, box.w - inset * 2.0f, 1.0f};
    sink.fillRounded(light, 1, Show::kGelEdge);
  }
  sink.pop();
}

// Specular crescent for white knobs (slider/switch thumbs): brightens
// the crown so the face reads glossy, not chalky. The rim stays a
// mid gray, never black — a black ring around white on near-black
// backgrounds is what reads as a dirty halo at native size.
inline void knobGloss(AnySink& sink, const Box& knob) {
  if (knob.w < 12.0f || knob.h < 12.0f) {
    return;
  }
  sink.pushBlend(BlendMode::Plus);
  Box crown{
      knob.x + knob.w * 0.25f,
      knob.y + knob.h * 0.12f,
      knob.w * 0.5f,
      knob.h * 0.32f
  };
  sink.fillRounded(crown, 3, Show::kGelGloss);
  sink.pop();
}

}  // namespace toolkit
}  // namespace glim
