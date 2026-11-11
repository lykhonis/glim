#pragma once

#include <glim/paint/Scene.h>
#include <glim/paint/Software.h>

#include <vector>

#include "RasterPixel.h"
#include "RasterPlate.h"

namespace glim::paint::raster {

void maskFrostPlate(std::vector<Pixel>& plate, int iw, int ih, const PlateSpec& spec);

PlateSpec glassPillsToField(
    const GlassPill* pills,
    int n,
    float mergeKPx,
    float pr,
    const Rect& surface,
    const RasterOptions& options
);

void sampleGlassPlate(
    std::vector<Pixel>& plate,
    int iw,
    int ih,
    const std::vector<Pixel>& dest,
    int dw,
    int dh,
    Rect src,
    const Glass& mat,
    const PlateSpec& field,
    const RasterOptions& options
);

}  // namespace glim::paint::raster
