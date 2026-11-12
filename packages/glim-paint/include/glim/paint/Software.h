#pragma once

#include <glim/paint/FramePacket.h>
#include <glim/paint/Scene.h>

#include <cstdint>

namespace glim::paint {

// CPU lens: the LensMath reference optics per pixel over a bilinear dest.
struct RasterOptions {
  bool glassBlur = false;   // optional Gaussian; off — Regular uses 5-tap
  bool glassMerge = false;  // smin morph; off — pills stay separate
};

void rasterScene(
    const Scene& scene, int width, int height, std::uint8_t* rgba, RasterOptions options = {}
);
void rasterPacket(
    const FramePacket& packet, int width, int height, std::uint8_t* rgba, RasterOptions options = {}
);

}  // namespace glim::paint
