#pragma once

#include <glim/paint/FramePacket.h>
#include <glim/paint/Scene.h>

#include <vector>

#include "RasterPixel.h"

namespace glim::paint::raster {

struct PlateSpec {
  float sigma = 0;
  float mergeK = 0;
  bool flat = false;
  float pr = 1.f;
  int n = 0;
  BackdropPill pills[kMaxBackdropPills]{};
};

Pixel samplePixel(const std::vector<Pixel>& img, int w, int h, float px, float py);

Pixel sampleDest(const std::vector<Pixel>& dest, int dw, int dh, float sx, float sy);

Pixel sampleDestBilinear(const std::vector<Pixel>& dest, int dw, int dh, float sx, float sy);

// A pill moved into plate space: origin-relative, scaled per axis.
BackdropPill plateSpacePill(const Rect& r, float radius, Vec2 origin, float sx, float sy);

PlateSpec specFromGroup(const Group& g, Rect surface, float pr);

PlateSpec specFromIsolate(const Isolate& iso, float pr);

void blurCheap(std::vector<Pixel>& img, int w, int h, float sigma);

void copyDestPlate(
  std::vector<Pixel>& plate,
  int iw,
  int ih,
  const std::vector<Pixel>& dest,
  int dw,
  int dh,
  Rect src
);

}  // namespace glim::paint::raster
