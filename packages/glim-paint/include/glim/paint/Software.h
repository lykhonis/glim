#pragma once

#include <cstdint>

#include <glim/paint/FramePacket.h>
#include <glim/paint/Scene.h>

namespace glim::paint {

// CPU glass: bilinear dest, cheap 5-tap frost, 1.5px AA, analytic rim. No lens.
struct RasterOptions {
    bool glassBlur = false;   // optional Gaussian; off — Regular uses 5-tap
    bool glassMerge = false;  // smin morph; off — pills stay separate
};

void rasterScene(const Scene& scene, int width, int height, std::uint8_t* rgba,
                 RasterOptions options = {});
void rasterPacket(const FramePacket& packet, int width, int height, std::uint8_t* rgba,
                  RasterOptions options = {});

}  // namespace glim::paint
