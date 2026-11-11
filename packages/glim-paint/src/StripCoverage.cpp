#include <algorithm>
#include <cmath>

#include "Strip.h"

namespace glim::paint {

float chamferCoverage(const Rect& rect, float cut, float px, float py) {
  if (rect.size.x <= 0.f || rect.size.y <= 0.f) {
    return 0.f;
  }
  const float x0 = rect.origin.x, y0 = rect.origin.y;
  const float x1 = x0 + rect.size.x, y1 = y0 + rect.size.y;
  float c = std::max(0.f, cut);
  c = std::min(c, std::min(rect.size.x, rect.size.y) * 0.5f);
  if (c <= 1e-4f) {
    return (px >= x0 && px < x1 && py >= y0 && py < y1) ? 1.f : 0.f;
  }
  // Octagon SDF: outside distance to rect minus corner triangles.
  // Inside rect?
  if (px < x0 || px >= x1 || py < y0 || py >= y1) {
    return 0.f;
  }
  // Distance to each diagonal cut (positive inside).
  // TL cut line: (x - x0) + (y - y0) = c
  const float dTL = (px - x0) + (py - y0) - c;
  const float dTR = (x1 - px) + (py - y0) - c;
  const float dBL = (px - x0) + (y1 - py) - c;
  const float dBR = (x1 - px) + (y1 - py) - c;
  const float dMin = std::min(std::min(dTL, dTR), std::min(dBL, dBR));
  // dMin is Manhattan-ish; convert to perpendicular distance (/sqrt2).
  const float outside = -dMin / 1.41421356f;
  const float aa = 0.5f - outside;
  if (aa <= 0.f) return 0.f;
  if (aa >= 1.f) return 1.f;
  return aa;
}

float arcCoverage(const Arc& a, float px, float py) {
  if (a.radius <= 0.f || a.thickness <= 0.f || a.sweepDeg == 0.f) {
    return 0.f;
  }
  const float dx = px - a.center.x, dy = py - a.center.y;
  const float r = std::hypot(dx, dy);
  const float half = a.thickness * 0.5f;
  if (a.sweepDeg >= 360.f) {
    // Full ring: radial only, no angular math.
    const float dr = std::fabs(r - a.radius) - half;
    const float aa = 0.5f - dr;
    if (aa <= 0.f) return 0.f;
    if (aa >= 1.f) return 1.f;
    return aa;
  }
  // Angular interval (degrees clockwise-on-screen like toolkit).
  // Convert point to degrees: atan2 in y-down gives clockwise from +x.
  float ang = std::atan2(dy, dx) * 57.29578f;
  if (ang < 0.f) ang += 360.f;
  float start = std::fmod(a.startDeg, 360.f);
  if (start < 0.f) start += 360.f;
  float sweep = a.sweepDeg;
  bool inAngle = false;
  float capDist = 1e9f;
  if (sweep >= 360.f) {
    inAngle = true;
  } else if (sweep > 0.f) {
    float end = start + sweep;
    float rel = ang - start;
    while (rel < 0.f) rel += 360.f;
    while (rel >= 360.f) rel -= 360.f;
    inAngle = rel <= sweep;
    // Distance to caps for round joins.
    const float s0 = start * 0.0174533f, s1 = (start + sweep) * 0.0174533f;
    const float c0x = a.center.x + std::cos(s0) * a.radius;
    const float c0y = a.center.y + std::sin(s0) * a.radius;
    const float c1x = a.center.x + std::cos(s1) * a.radius;
    const float c1y = a.center.y + std::sin(s1) * a.radius;
    if (!inAngle) {
      // Beyond sweep: distance to nearest cap decides round cap.
      const float d0 = std::hypot(px - c0x, py - c0y);
      const float d1 = std::hypot(px - c1x, py - c1y);
      capDist = std::min(d0, d1) - half;
      const float aa = 0.5f - capDist;
      if (aa <= 0.f) return 0.f;
      if (aa >= 1.f) return 1.f;
      return aa;
    }
    (void)end;
  } else {
    return 0.f;
  }
  const float dr = std::fabs(r - a.radius) - half;
  const float aa = 0.5f - dr;
  if (aa <= 0.f) return 0.f;
  if (aa >= 1.f) return 1.f;
  return aa;
}

float segDist(Vec2 p, Vec2 a, Vec2 b, float& tOut, float& lenOut) {
  const float abx = b.x - a.x, aby = b.y - a.y;
  const float l2 = abx * abx + aby * aby;
  float t = 0.f;
  if (l2 > 1e-12f) {
    t = ((p.x - a.x) * abx + (p.y - a.y) * aby) / l2;
    t = std::max(0.f, std::min(1.f, t));
  }
  const float qx = a.x + abx * t, qy = a.y + aby * t;
  tOut = t;
  lenOut = std::sqrt(l2);
  return std::hypot(p.x - qx, p.y - qy);
}

float polylineCoverage(
    const Vec2* pts, std::size_t n, float width, float dotPitch, float px, float py
) {
  if (n < 2 || width <= 0.f) return 0.f;
  const float half = width * 0.5f;
  const Vec2 p{px, py};
  if (dotPitch <= 0.f) {
    float best = 1e9f;
    for (std::size_t i = 0; i + 1 < n; ++i) {
      float t = 0.f, len = 0.f;
      const float d = segDist(p, pts[i], pts[i + 1], t, len);
      best = std::min(best, d);
    }
    const float aa = 0.5f - (best - half);
    if (aa <= 0.f) return 0.f;
    if (aa >= 1.f) return 1.f;
    return aa;
  }
  // Dotted: dots of diameter=width spaced by dotPitch along path.
  // NOTE: this per-pixel form is O(pixels × dots) — only for tiny
  // queries. Bulk raster uses flattenDottedPolyline (dot stamping).
  float best = 1e9f;
  float acc = 0.f;
  for (std::size_t i = 0; i + 1 < n; ++i) {
    const float abx = pts[i + 1].x - pts[i].x, aby = pts[i + 1].y - pts[i].y;
    const float segLen = std::hypot(abx, aby);
    if (segLen <= 1e-9f) continue;
    // Walk dots along this segment.
    // First dot offset so dots are globally periodic from path start.
    float startOff = 0.f;
    {
      const float k = std::ceil(acc / dotPitch);
      startOff = k * dotPitch - acc;
    }
    for (float d = startOff; d <= segLen; d += dotPitch) {
      const float t = segLen > 0.f ? d / segLen : 0.f;
      const float cx = pts[i].x + abx * t, cy = pts[i].y + aby * t;
      best = std::min(best, std::hypot(px - cx, py - cy));
    }
    acc += segLen;
  }
  const float aa = 0.5f - (best - half);
  if (aa <= 0.f) return 0.f;
  if (aa >= 1.f) return 1.f;
  return aa;
}

float roundedCoverage(const Rect& rect, Radius radius, float px, float py) {
  if (rect.size.x <= 0.f || rect.size.y <= 0.f) {
    return 0.f;
  }
  radius = clampRadius(rect, radius);
  const float x = px - (rect.origin.x + rect.size.x * 0.5f);
  const float y = py - (rect.origin.y + rect.size.y * 0.5f);
  const float hw = rect.size.x * 0.5f;
  const float hh = rect.size.y * 0.5f;
  float r = 0.f;
  if (x < 0.f && y < 0.f) {
    r = radius.lt;
  } else if (x >= 0.f && y < 0.f) {
    r = radius.rt;
  } else if (x < 0.f && y >= 0.f) {
    r = radius.lb;
  } else {
    r = radius.rb;
  }
  const float qx = std::fabs(x) - hw + r;
  const float qy = std::fabs(y) - hh + r;
  const float ox = std::max(qx, 0.f);
  const float oy = std::max(qy, 0.f);
  const float outside = std::hypot(ox, oy) + std::min(std::max(qx, qy), 0.f) - r;
  // 0.5 - outside is the exact area fraction (matches fillQuad); narrowing it desyncs CPU/GPU.
  // Crispness must come from device pixels. Same factor in all GPU rounded shaders.
  const float aa = 0.5f - outside;
  if (aa <= 0.f) {
    return 0.f;
  }
  if (aa >= 1.f) {
    return 1.f;
  }
  return aa;
}

}  // namespace glim::paint
