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
    std::uint8_t sdf = 0;
};

// One coverage-weighted span of a gradient-filled shape, in the same logical
// space as Quad. Stops are premultiplied RGBA evaluated piecewise-linearly
// over t in [0,1]: linear projects onto p0->p1, radial uses |p-c|/radius.
struct GradientQuad {
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    float coverage = 1;
    std::uint8_t kind = 0;  // 0 = linear, 1 = radial
    float p0x = 0;
    float p0y = 0;
    float p1x = 0;
    float p1y = 0;
    float radius = 0;
    std::uint8_t stopCount = 0;
    float offsets[kMaxGradientStops]{};
    float r[kMaxGradientStops]{};
    float g[kMaxGradientStops]{};
    float b[kMaxGradientStops]{};
    float a[kMaxGradientStops]{};
};

struct Isolate {
    float destX = 0;
    float destY = 0;
    float destW = 0;
    float destH = 0;
    float opacity = 1;
    int contentW = 1;
    int contentH = 1;
    float backdropSigma = 0;
    float backdropBend = 0;
    float backdropRadius = 0;
    float backdropMerge = 0;
    float backdropPress = 0;
    float backdropLightX = 0.35f;
    float backdropLightY = 0.8f;
    float backdropLightZ = 0.5f;
    float backdropU0 = 0;
    float backdropV0 = 0;
    float backdropU1 = 1;
    float backdropV1 = 1;
    bool backdropFlat = false;
    std::vector<BackdropPill> backdropPills;
    bool hasGlass = false;
    bool glassContainer = false;
    Glass glass{};
    std::vector<GlassPill> glassPills;
    float glassU0 = 0;
    float glassV0 = 0;
    float glassU1 = 1;
    float glassV1 = 1;
    std::vector<Quad> quads;
    std::vector<BlitQuad> blits;
    std::vector<GradientQuad> gradients;
    std::vector<Isolate> isolates;
};

struct FramePacket {
    Vec2 logicalSize;
    std::vector<Quad> quads;
    std::vector<BlitQuad> blits;
    std::vector<GradientQuad> gradients;
    std::vector<Isolate> isolates;
    ImageStore images;
    Stats stats;
};

FramePacket encode(const Scene& scene, float pixelRatio = 1.0f);

}  // namespace glim::paint
