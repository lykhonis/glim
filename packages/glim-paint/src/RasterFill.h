#pragma once

#include <glim/paint/Scene.h>

#include <vector>

#include "RasterPixel.h"
#include "Strip.h"

namespace glim::paint::raster {

void fillQuad(
    std::vector<Pixel>& buf, int w, int h, const Quad& q, Blend fallback = Blend::SrcOver
);

void fillQuads(
    std::vector<Pixel>& buf,
    int w,
    int h,
    const std::vector<Quad>& quads,
    Blend fallback = Blend::SrcOver
);

void fillGradients(
    std::vector<Pixel>& buf,
    int w,
    int h,
    const std::vector<GradientQuad>& grads,
    Blend fallback = Blend::SrcOver
);

void blitBuffer(
    std::vector<Pixel>& dest,
    int dw,
    int dh,
    const std::vector<Pixel>& src,
    int sw,
    int sh,
    Rect dstRect,
    float opacity
);

void blitImage(
    std::vector<Pixel>& dest,
    int w,
    int h,
    const Blit& b,
    const ImageStore& images,
    Vec4 tint,
    const ClipState& clip,
    bool sdf,
    Blend blend = Blend::SrcOver
);

}  // namespace glim::paint::raster
