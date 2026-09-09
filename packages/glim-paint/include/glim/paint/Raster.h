#pragma once

#include <cstdint>
#include <vector>

#include <glim/paint/Scene.h>

namespace glim::paint {

// CPU reference: premul src-over, Y-down, integer pixel coverage. pixelRatio=1.
void raster(const Scene& scene, int width, int height, std::uint8_t* rgba);

bool writeRgbaPng(const char* path, int width, int height, const std::uint8_t* rgba);
bool readRgbaPng(const char* path, int* width, int* height, std::vector<std::uint8_t>* rgba);

}  // namespace glim::paint
