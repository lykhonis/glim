#pragma once

#include <cstdint>

#include <glim/paint/FramePacket.h>
#include <glim/paint/Scene.h>

namespace glim::paint {

void rasterScene(const Scene& scene, int width, int height, std::uint8_t* rgba);
void rasterPacket(const FramePacket& packet, int width, int height, std::uint8_t* rgba);

}  // namespace glim::paint
