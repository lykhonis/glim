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
void flattenSolidPolyline(const Vec2*, std::size_t n, float width, std::vector<Strip>&);
void clipStrips(std::vector<Strip>&, const ClipState&);
void stripsToQuads(const std::vector<Strip>&, const Vec4& premul, std::vector<Quad>&);
void stripsToGradientQuads(const std::vector<Strip>&, const Matter&, std::vector<GradientQuad>&);
void appendGradientQuad(
  std::vector<GradientQuad>& out,
  float x,
  float y,
  float w,
  float h,
  float coverage,
  const Matter& m
);
void flattenDottedPolyline(
  const Vec2* pts, std::size_t n, float width, float dotPitch, std::vector<Strip>& out
);

void appendShape(std::vector<Quad>& quads, std::vector<BlitQuad>& blits, std::vector<GradientQuad>& gradients, const Shape& shape, const ClipState&);

// World-aware append: axis-aligned worlds take the fast baked path;
// rotated worlds rasterize via inverse-mapped coverage (true rotated
// quads as 1px strips — no new shape kinds, works on CPU + GPU).
void appendShapeWorld(
  std::vector<Quad>& quads,
  std::vector<BlitQuad>& blits,
  std::vector<GradientQuad>& gradients,
  const Shape& shape,
  const Mat4& world,
  const ClipState& clip
);

float roundedCoverage(const Rect&, Radius, float px, float py);
float chamferCoverage(const Rect&, float cut, float px, float py);
float arcCoverage(const Arc&, float px, float py);
float polylineCoverage(const Vec2*, std::size_t n, float width, float dotPitch, float px, float py);
float segDist(Vec2 p, Vec2 a, Vec2 b, float& tOut, float& lenOut);

}  // namespace glim::paint
