#pragma once

#include <vector>

#include <glim/math.h>
#include <glim/paint/Scene.h>

namespace glim::paint {

// Premultiplied quad in logical pixels. This is the CPU→GPU (and future
// WASM→native) unit of work. No GPU types.
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

// Isolated group: contents are in local 0..contentW x 0..contentH; blit to dest
// in the parent with opacity. Nested isolates allowed.
struct Isolate {
    float destX = 0;
    float destY = 0;
    float destW = 0;
    float destH = 0;
    float opacity = 1;
    int contentW = 1;
    int contentH = 1;
    std::vector<Quad> quads;
    std::vector<Isolate> isolates;
};

// One frame after merge. Safe to copy across a WASM linear-memory boundary
// once a compact serializer exists; until then this is the in-process contract.
struct FramePacket {
    Vec2 logicalSize;
    std::vector<Quad> quads;
    std::vector<Isolate> isolates;
    Stats stats;
};

FramePacket encode(const Scene& scene, float pixelRatio = 1.0f);

}  // namespace glim::paint
