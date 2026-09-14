#pragma once

#include <glim/paint/FramePacket.h>
#include <glim/paint/Scene.h>

#include <vector>

namespace glim::paint {

constexpr int kStripHeight = 4;

struct ClipState {
    bool active = false;
    Rect rect{};
    Radius radius{};
};

struct StripSpan {
    float x0 = 0;
    float x1 = 0;
    float coverage = 1;
};

struct Strip {
    float y = 0;
    float height = static_cast<float>(kStripHeight);
    std::vector<StripSpan> spans;
};

ClipState clipOf(const GroupParams&, const Mat4& extra);
ClipState intersectClip(const ClipState& parent, const ClipState& child);
Radius clampRadius(const Rect&, Radius);
Rect intersectRect(Rect, Rect);
Rect strokeBounds(const Stroke&);

void flattenRounded(const Rect&, Radius, std::vector<Strip>&);
void flattenStroke(const Rect&, Radius, float width, std::vector<Strip>&);
void clipStrips(std::vector<Strip>&, const ClipState&);
void stripsToQuads(const std::vector<Strip>&, const Vec4& premul, std::vector<Quad>&);

void appendShape(std::vector<Quad>& quads, std::vector<BlitQuad>& blits, const Shape&,
                 const ClipState&);

float roundedCoverage(const Rect&, Radius, float px, float py);

}  // namespace glim::paint
