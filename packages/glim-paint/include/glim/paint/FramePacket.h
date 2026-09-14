#pragma once

#include <cstdint>
#include <vector>

#include <glim/math.h>
#include <glim/paint/Scene.h>

namespace glim::paint {

struct Quad {
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    float r = 0;
    float g = 0;
    float b = 0;
    float a = 0;
};

struct BlitQuad {
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    float u0 = 0;
    float v0 = 0;
    float u1 = 1;
    float v1 = 1;
    float r = 1;
    float g = 1;
    float b = 1;
    float a = 1;
    std::uint32_t imageId = 0;
};

struct Isolate {
    float destX = 0;
    float destY = 0;
    float destW = 0;
    float destH = 0;
    float opacity = 1;
    int contentW = 1;
    int contentH = 1;
    std::vector<Quad> quads;
    std::vector<BlitQuad> blits;
    std::vector<Isolate> isolates;
};

struct FramePacket {
    Vec2 logicalSize;
    std::vector<Quad> quads;
    std::vector<BlitQuad> blits;
    std::vector<Isolate> isolates;
    ImageStore images;
    Stats stats;
};

FramePacket encode(const Scene& scene, float pixelRatio = 1.0f);

}  // namespace glim::paint
