#include "Strip.h"

#include <glim/paint/ColorSpace.h>

#include <algorithm>
#include <cmath>

namespace glim::paint {
namespace {

constexpr float kEps = 1e-4f;

float scaleOf(const Mat4& t, const Rect& r) {
  if (r.size.x <= kEps || r.size.y <= kEps) {
    const Vec4 o = t * Vec4{0, 0, 0, 1};
    const Vec4 x = t * Vec4{1, 0, 0, 1};
    const Vec4 y = t * Vec4{0, 1, 0, 1};
    const float sx = std::hypot(x.x - o.x, x.y - o.y);
    const float sy = std::hypot(y.x - o.x, y.y - o.y);
    return std::min(sx, sy);
  }
  const Rect xf = transformRect(t, r);
  const float sx = std::fabs(xf.size.x / r.size.x);
  const float sy = std::fabs(xf.size.y / r.size.y);
  return std::min(sx, sy);
}

Radius scaleRadius(Radius r, float s) {
  r.lt *= s;
  r.rt *= s;
  r.lb *= s;
  r.rb *= s;
  return r;
}

bool sharp(const Radius& r) { return r.lt <= kEps && r.rt <= kEps && r.lb <= kEps && r.rb <= kEps; }

void addSpan(Strip& strip, float x0, float x1, float coverage) {
  if (x1 <= x0 + 1e-6f || coverage <= 1e-4f) {
    return;
  }
  if (!strip.spans.empty()) {
    StripSpan& last = strip.spans.back();
    if (std::fabs(last.coverage - coverage) <= 1e-3f && x0 <= last.x1 + 1e-3f) {
      last.x1 = std::max(last.x1, x1);
      return;
    }
  }
  strip.spans.push_back({x0, x1, coverage});
}

void packHeight1(std::vector<Strip>& strips) {
  if (strips.size() < 2) {
    return;
  }
  std::vector<Strip> packed;
  packed.reserve(strips.size());
  std::size_t i = 0;
  while (i < strips.size()) {
    Strip cur = std::move(strips[i]);
    ++i;
    while (i < strips.size() && std::fabs(cur.height - 1.f) <= kEps &&
           std::fabs(strips[i].height - 1.f) <= kEps &&
           std::fabs(strips[i].y - (cur.y + cur.height)) <= kEps &&
           cur.spans.size() == strips[i].spans.size() &&
           cur.height + 1.f <= static_cast<float>(kStripHeight) + kEps) {
      bool same = true;
      for (std::size_t s = 0; s < cur.spans.size(); ++s) {
        if (std::fabs(cur.spans[s].x0 - strips[i].spans[s].x0) > 1e-3f ||
            std::fabs(cur.spans[s].x1 - strips[i].spans[s].x1) > 1e-3f ||
            std::fabs(cur.spans[s].coverage - strips[i].spans[s].coverage) > 1e-3f) {
          same = false;
          break;
        }
      }
      if (!same) {
        break;
      }
      cur.height += 1.f;
      ++i;
    }
    packed.push_back(std::move(cur));
  }
  strips = std::move(packed);
}

// Pixel columns [a, b) of one row that may carry coverage.
struct ColumnRange {
  int a, b;
};

// Coverage sampled per row over `rows(y, ranges)` windows (at most two);
// pixels outside the windows must have zero coverage.
template <typename Rows, typename Cov>
void flattenCoverageRows(
    int x0, int y0, int x1, int y1, std::vector<Strip>& out, Rows&& rows, Cov&& covAt
) {
  if (y1 > y0) out.reserve(out.size() + static_cast<std::size_t>(y1 - y0));
  for (int y = y0; y < y1; ++y) {
    Strip s;
    s.y = static_cast<float>(y);
    s.height = 1.f;
    s.spans.reserve(4);
    ColumnRange ranges[2];
    const int n = rows(y, ranges);
    for (int k = 0; k < n; ++k) {
      const int ra = std::max(x0, ranges[k].a), rb = std::min(x1, ranges[k].b);
      int runX = ra;
      float runCov = -1.f;
      auto flush = [&](int x) {
        if (runCov > 1e-4f) {
          addSpan(s, static_cast<float>(runX), static_cast<float>(x), runCov);
        }
        runCov = -1.f;
      };
      for (int x = ra; x < rb; ++x) {
        float cov = covAt(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
        if (cov >= 1.f - 1e-3f) {
          cov = 1.f;
        } else if (cov <= 1e-4f) {
          cov = 0.f;
        }
        if (runCov < 0.f) {
          runX = x;
          runCov = cov;
        } else if (std::fabs(cov - runCov) <= 1e-3f) {
          continue;
        } else {
          flush(x);
          runX = x;
          runCov = cov;
        }
      }
      flush(rb);
    }
    if (!s.spans.empty()) {
      out.push_back(std::move(s));
    }
  }
  packHeight1(out);
}

template <typename Cov>
void flattenCoverage(int x0, int y0, int x1, int y1, std::vector<Strip>& out, Cov&& covAt) {
  flattenCoverageRows(
      x0,
      y0,
      x1,
      y1,
      out,
      [&](int, ColumnRange* r) {
        r[0] = {x0, x1};
        return 1;
      },
      covAt
  );
}

BlitQuad toBlitQuad(const Blit& b) {
  const Vec4 p = premulLinear(b.matter.color);
  BlitQuad q;
  q.x = b.rect.origin.x;
  q.y = b.rect.origin.y;
  q.w = b.rect.size.x;
  q.h = b.rect.size.y;
  q.u0 = b.matter.uv.origin.x;
  q.v0 = b.matter.uv.origin.y;
  q.u1 = b.matter.uv.origin.x + b.matter.uv.size.x;
  q.v1 = b.matter.uv.origin.y + b.matter.uv.size.y;
  q.r = p.x;
  q.g = p.y;
  q.b = p.z;
  q.a = p.w;
  q.imageId = b.matter.imageId;
  q.sdf = 0;
  return q;
}

void appendGlyphs(std::vector<BlitQuad>& blits, const GlyphRun& run) {
  blits.reserve(blits.size() + run.glyphs.size());
  const Vec4 p = premulLinear(run.color);
  for (const GlyphQuad& g : run.glyphs) {
    BlitQuad q;
    q.x = g.dest.origin.x;
    q.y = g.dest.origin.y;
    q.w = g.dest.size.x;
    q.h = g.dest.size.y;
    q.u0 = g.uv.origin.x;
    q.v0 = g.uv.origin.y;
    q.u1 = g.uv.origin.x + g.uv.size.x;
    q.v1 = g.uv.origin.y + g.uv.size.y;
    q.r = p.x;
    q.g = p.y;
    q.b = p.z;
    q.a = p.w;
    q.imageId = run.imageId;
    q.sdf = 1;
    blits.push_back(q);
  }
}

}  // namespace

Radius clampRadius(const Rect& rect, Radius r) {
  r.lt = std::max(0.f, r.lt);
  r.rt = std::max(0.f, r.rt);
  r.lb = std::max(0.f, r.lb);
  r.rb = std::max(0.f, r.rb);
  const float w = std::max(0.f, rect.size.x);
  const float h = std::max(0.f, rect.size.y);
  const float top = r.lt + r.rt;
  if (top > w && top > 0.f) {
    const float s = w / top;
    r.lt *= s;
    r.rt *= s;
  }
  const float bot = r.lb + r.rb;
  if (bot > w && bot > 0.f) {
    const float s = w / bot;
    r.lb *= s;
    r.rb *= s;
  }
  const float left = r.lt + r.lb;
  if (left > h && left > 0.f) {
    const float s = h / left;
    r.lt *= s;
    r.lb *= s;
  }
  const float right = r.rt + r.rb;
  if (right > h && right > 0.f) {
    const float s = h / right;
    r.rt *= s;
    r.rb *= s;
  }
  return r;
}

Rect intersectRect(Rect a, Rect b) {
  const float x0 = std::max(a.origin.x, b.origin.x);
  const float y0 = std::max(a.origin.y, b.origin.y);
  const float x1 = std::min(a.origin.x + a.size.x, b.origin.x + b.size.x);
  const float y1 = std::min(a.origin.y + a.size.y, b.origin.y + b.size.y);
  if (x1 <= x0 || y1 <= y0) {
    return {};
  }
  return {{x0, y0}, {x1 - x0, y1 - y0}};
}

Rect strokeBounds(const Stroke& s) {
  const float o = std::max(0.f, s.width) * 0.5f;
  return {
      {s.rect.origin.x - o, s.rect.origin.y - o}, {s.rect.size.x + o * 2.f, s.rect.size.y + o * 2.f}
  };
}

ClipState clipOf(const GroupParams& p, const Mat4& extra) {
  if (!hasClip(p)) {
    return {};
  }
  ClipState c;
  c.active = true;
  c.rect = transformRect(extra, p.clip);
  c.radius = scaleRadius(p.clipRadius, scaleOf(extra, p.clip));
  return c;
}

ClipState intersectClip(const ClipState& parent, const ClipState& child) {
  if (!parent.active) {
    return child;
  }
  if (!child.active) {
    return parent;
  }
  ClipState out;
  out.active = true;
  out.rect = intersectRect(parent.rect, child.rect);
  if (out.rect.size.x <= 0.f || out.rect.size.y <= 0.f) {
    out.active = true;
    out.rect = {};
    return out;
  }
  if (sharp(parent.radius) || sharp(child.radius)) {
    out.radius = {};
  } else {
    out.radius = parent.radius;
    out.radius.lt = std::min(parent.radius.lt, child.radius.lt);
    out.radius.rt = std::min(parent.radius.rt, child.radius.rt);
    out.radius.lb = std::min(parent.radius.lb, child.radius.lb);
    out.radius.rb = std::min(parent.radius.rb, child.radius.rb);
  }
  return out;
}

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

namespace {
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
}  // namespace

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

inline float discCoverage(float cx, float cy, float r, float px, float py) {
  const float aa = 0.5f - (std::hypot(px - cx, py - cy) - r);
  if (aa <= 0.f) return 0.f;
  if (aa >= 1.f) return 1.f;
  return aa;
}

void flattenDottedPolyline(
    const Vec2* pts, std::size_t n, float width, float dotPitch, std::vector<Strip>& out
) {
  if (n < 2 || width <= 0.f || dotPitch <= 0.f) return;
  out.reserve(out.size() + n * 4);
  const float r = width * 0.5f;
  float acc = 0.f;
  for (std::size_t i = 0; i + 1 < n; ++i) {
    const float abx = pts[i + 1].x - pts[i].x, aby = pts[i + 1].y - pts[i].y;
    const float segLen = std::hypot(abx, aby);
    if (segLen <= 1e-9f) continue;
    const float startOff = std::ceil(acc / dotPitch) * dotPitch - acc;
    for (float d = startOff; d <= segLen; d += dotPitch) {
      const float t = d / segLen;
      const float cx = pts[i].x + abx * t, cy = pts[i].y + aby * t;
      const int x0 = static_cast<int>(std::floor(cx - r)) - 1;
      const int y0 = static_cast<int>(std::floor(cy - r)) - 1;
      const int x1 = static_cast<int>(std::ceil(cx + r)) + 1;
      const int y1 = static_cast<int>(std::ceil(cy + r)) + 1;
      flattenCoverage(x0, y0, x1, y1, out, [&](float px, float py) {
        return discCoverage(cx, cy, r, px, py);
      });
    }
    acc += segLen;
  }
  packHeight1(out);
}

// Solid polyline, row-banded: a segment only covers pixels within half+0.5 of it,
// so each row evaluates only segments whose padded y-range holds the row, over
// their padded x-intervals. Skipped pixels have zero coverage; spans match a full
// bbox scan of polylineCoverage exactly.
void flattenSolidPolyline(const Vec2* pts, std::size_t n, float width, std::vector<Strip>& out) {
  if (n < 2 || width <= 0.f) return;
  const float half = width * 0.5f, pad = half + 1.f;
  float top = pts[0].y, bottom = pts[0].y;
  for (std::size_t i = 1; i < n; ++i)
    top = std::min(top, pts[i].y), bottom = std::max(bottom, pts[i].y);
  const int y0 = static_cast<int>(std::floor(top - pad));
  const int y1 = static_cast<int>(std::ceil(bottom + pad));
  struct Reach {
    std::size_t segment;
    int lo, hi;
  };
  std::vector<Reach> active;
  std::vector<std::pair<int, int>> spans;
  for (int y = y0; y < y1; ++y) {
    const float cy = static_cast<float>(y) + 0.5f;
    active.clear();
    spans.clear();
    for (std::size_t i = 0; i + 1 < n; ++i) {
      const Vec2 a = pts[i], b = pts[i + 1];
      if (cy < std::min(a.y, b.y) - pad || cy > std::max(a.y, b.y) + pad) continue;
      // Only the part of the segment within pad of this row can cover its pixels, so
      // distances are evaluated over that reach alone (long diagonals cost width x
      // height otherwise). The scan keeps the whole x-range: run merging below
      // depends on it, and pixels out of every reach are zero anyway.
      float t0 = 0.f, t1 = 1.f;
      const float dy = b.y - a.y;
      if (std::fabs(dy) > 1e-6f) {
        const float ta = (cy - pad - a.y) / dy, tb = (cy + pad - a.y) / dy;
        t0 = std::max(0.f, std::min(ta, tb));
        t1 = std::min(1.f, std::max(ta, tb));
      }
      const float xa = a.x + (b.x - a.x) * t0, xb = a.x + (b.x - a.x) * t1;
      // One extra pixel absorbs rounding in the clip; it only adds zero coverage.
      const int lo = static_cast<int>(std::floor(std::min(xa, xb) - pad - 1.f));
      const int hi = static_cast<int>(std::ceil(std::max(xa, xb) + pad + 1.f));
      active.push_back({i, lo, hi});
      spans.emplace_back(
          static_cast<int>(std::floor(std::min(a.x, b.x) - pad)),
          static_cast<int>(std::ceil(std::max(a.x, b.x) + pad))
      );
    }
    if (active.empty()) continue;
    std::sort(spans.begin(), spans.end());
    Strip s;
    s.y = static_cast<float>(y);
    s.height = 1.f;
    std::size_t k = 0;
    while (k < spans.size()) {
      int lo = spans[k].first, hi = spans[k].second;
      for (++k; k < spans.size() && spans[k].first <= hi; ++k) hi = std::max(hi, spans[k].second);
      int runX = lo;
      float runCov = 0.f;
      for (int x = lo; x <= hi; ++x) {
        float cov = 0.f;
        if (x < hi) {
          float best = 1e9f;
          const Vec2 p{static_cast<float>(x) + 0.5f, cy};
          for (const Reach& r : active) {
            // A segment out of reach is over pad away: it never sets coverage.
            if (x < r.lo || x > r.hi) continue;
            float t = 0.f, len = 0.f;
            best = std::min(best, segDist(p, pts[r.segment], pts[r.segment + 1], t, len));
          }
          cov = std::clamp(0.5f - (best - half), 0.f, 1.f);
          if (cov >= 1.f - 1e-3f)
            cov = 1.f;
          else if (cov <= 1e-4f)
            cov = 0.f;
        }
        if (x == lo) {
          runCov = cov;
          continue;
        }
        if (x < hi && std::fabs(cov - runCov) <= 1e-3f) continue;
        if (runCov > 1e-4f) addSpan(s, static_cast<float>(runX), static_cast<float>(x), runCov);
        runX = x;
        runCov = cov;
      }
    }
    if (!s.spans.empty()) out.push_back(std::move(s));
  }
  packHeight1(out);
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

void flattenRounded(const Rect& rect, Radius radius, std::vector<Strip>& out) {
  if (rect.size.x <= 0.f || rect.size.y <= 0.f) {
    return;
  }
  radius = clampRadius(rect, radius);
  const auto emitRect = [&](Rect r) {
    if (r.size.x <= 0.f || r.size.y <= 0.f) {
      return;
    }
    Strip s;
    s.y = r.origin.y;
    s.height = r.size.y;
    addSpan(s, r.origin.x, r.origin.x + r.size.x, 1.f);
    out.push_back(std::move(s));
  };
  const auto touches = [&](Rect r, float px, float py) {
    return px >= r.origin.x && px <= r.origin.x + r.size.x && py >= r.origin.y &&
           py <= r.origin.y + r.size.y;
  };
  if (sharp(radius)) {
    emitRect(rect);
    return;
  }
  const float iL = std::max(radius.lt, radius.lb);
  const float iR = std::max(radius.rt, radius.rb);
  const float iT = std::max(radius.lt, radius.rt);
  const float iB = std::max(radius.lb, radius.rb);
  // Snap shared edges to integer pixels; complementary SrcOver fractions would otherwise seam.
  float sx1 = std::round(rect.origin.x + iL);
  float sx2 = std::round(rect.origin.x + rect.size.x - iR);
  float sy1 = std::round(rect.origin.y + iT);
  float sy2 = std::round(rect.origin.y + rect.size.y - iB);
  if (sx2 <= sx1 || sy2 <= sy1) {
    sx1 = rect.origin.x + iL;
    sx2 = rect.origin.x + rect.size.x - iR;
    sy1 = rect.origin.y + iT;
    sy2 = rect.origin.y + rect.size.y - iB;
  }
  const Rect body{{sx1, sy1}, {sx2 - sx1, sy2 - sy1}};
  const Rect top{
      {rect.origin.x + radius.lt, rect.origin.y},
      {rect.size.x - radius.lt - radius.rt, sy1 - rect.origin.y}
  };
  const Rect bot{
      {rect.origin.x + radius.lb, sy2},
      {rect.size.x - radius.lb - radius.rb, rect.origin.y + rect.size.y - sy2}
  };
  const Rect left{
      {rect.origin.x, rect.origin.y + radius.lt},
      {sx1 - rect.origin.x, rect.size.y - radius.lt - radius.lb}
  };
  const Rect right{
      {sx2, rect.origin.y + radius.rt},
      {rect.origin.x + rect.size.x - sx2, rect.size.y - radius.rt - radius.rb}
  };
  emitRect(body);
  emitRect(top);
  emitRect(bot);
  emitRect(left);
  emitRect(right);
  const auto corner = [&](float cx, float cy, float cw, float ch) {
    if (cw <= 0.f || ch <= 0.f) {
      return;
    }
    const int x0 = static_cast<int>(std::floor(cx)) - 1;
    const int y0 = static_cast<int>(std::floor(cy)) - 1;
    const int x1 = static_cast<int>(std::ceil(cx + cw)) + 1;
    const int y1 = static_cast<int>(std::ceil(cy + ch)) + 1;
    for (int y = y0; y < y1; ++y) {
      Strip s;
      s.y = static_cast<float>(y);
      s.height = 1.f;
      for (int x = x0; x < x1; ++x) {
        const float px = static_cast<float>(x) + 0.5f;
        const float py = static_cast<float>(y) + 0.5f;
        const bool inTiled =
            (touches(body, x, y) || touches(top, x, y) || touches(bot, x, y) ||
             touches(left, x, y) || touches(right, x, y)) &&
            (touches(body, x + 1.f, y) || touches(top, x + 1.f, y) || touches(bot, x + 1.f, y) ||
             touches(left, x + 1.f, y) || touches(right, x + 1.f, y)) &&
            (touches(body, x, y + 1.f) || touches(top, x, y + 1.f) || touches(bot, x, y + 1.f) ||
             touches(left, x, y + 1.f) || touches(right, x, y + 1.f)) &&
            (touches(body, x + 1.f, y + 1.f) || touches(top, x + 1.f, y + 1.f) ||
             touches(bot, x + 1.f, y + 1.f) || touches(left, x + 1.f, y + 1.f) ||
             touches(right, x + 1.f, y + 1.f));
        if (inTiled) {
          continue;
        }
        addSpan(
            s,
            static_cast<float>(x),
            static_cast<float>(x + 1),
            roundedCoverage(rect, radius, px, py)
        );
      }
      if (!s.spans.empty()) {
        out.push_back(std::move(s));
      }
    }
  };
  corner(rect.origin.x, rect.origin.y, radius.lt, radius.lt);
  corner(rect.origin.x + rect.size.x - radius.rt, rect.origin.y, radius.rt, radius.rt);
  corner(rect.origin.x, rect.origin.y + rect.size.y - radius.lb, radius.lb, radius.lb);
  corner(
      rect.origin.x + rect.size.x - radius.rb,
      rect.origin.y + rect.size.y - radius.rb,
      radius.rb,
      radius.rb
  );
}

void flattenStroke(const Rect& rect, Radius radius, float width, std::vector<Strip>& out) {
  if (width <= kEps || rect.size.x < 0.f || rect.size.y < 0.f) {
    return;
  }
  const float half = width * 0.5f;
  const Rect outer{
      {rect.origin.x - half, rect.origin.y - half}, {rect.size.x + width, rect.size.y + width}
  };
  Radius orad = radius;
  orad.lt += half;
  orad.rt += half;
  orad.lb += half;
  orad.rb += half;
  const Rect inner{
      {rect.origin.x + half, rect.origin.y + half}, {rect.size.x - width, rect.size.y - width}
  };
  const bool hole = inner.size.x > kEps && inner.size.y > kEps;
  Radius irad{};
  if (hole) {
    irad.lt = std::max(0.f, radius.lt - half);
    irad.rt = std::max(0.f, radius.rt - half);
    irad.lb = std::max(0.f, radius.lb - half);
    irad.rb = std::max(0.f, radius.rb - half);
  }
  const int x0 = static_cast<int>(std::floor(outer.origin.x)) - 1;
  const int y0 = static_cast<int>(std::floor(outer.origin.y)) - 1;
  const int x1 = static_cast<int>(std::ceil(outer.origin.x + outer.size.x)) + 1;
  const int y1 = static_cast<int>(std::ceil(outer.origin.y + outer.size.y)) + 1;
  auto covAt = [&](float px, float py) {
    const float outerCov = roundedCoverage(outer, orad, px, py);
    if (!hole) {
      return outerCov;
    }
    return std::max(0.f, outerCov - roundedCoverage(inner, irad, px, py));
  };
  if (!hole) {
    flattenCoverage(x0, y0, x1, y1, out, covAt);
    return;
  }
  // Between the inner corners, pixels more than a pixel inside the hole
  // are covered by both rects and so carry nothing: rows there only
  // sample the two side bands, keeping the cost on the perimeter.
  const float bandTop = inner.origin.y + std::max(irad.lt, irad.rt) + 1.5f;
  const float bandBottom = inner.origin.y + inner.size.y - std::max(irad.lb, irad.rb) - 1.5f;
  const int holeA = static_cast<int>(std::ceil(inner.origin.x)) + 2;
  const int holeB = static_cast<int>(std::floor(inner.origin.x + inner.size.x)) - 2;
  flattenCoverageRows(
      x0,
      y0,
      x1,
      y1,
      out,
      [&](int y, ColumnRange* r) {
        const float py = static_cast<float>(y) + 0.5f;
        if (py < bandTop || py > bandBottom || holeA >= holeB) {
          r[0] = {x0, x1};
          return 1;
        }
        r[0] = {x0, holeA};
        r[1] = {holeB, x1};
        return 2;
      },
      covAt
  );
}

void clipStrips(std::vector<Strip>& strips, const ClipState& clip) {
  if (!clip.active) {
    return;
  }
  if (clip.rect.size.x <= 0.f || clip.rect.size.y <= 0.f) {
    strips.clear();
    return;
  }
  std::vector<Strip> clipMask;
  flattenRounded(clip.rect, clip.radius, clipMask);
  std::vector<Strip> out;
  out.reserve(strips.size());
  for (const Strip& s : strips) {
    const float sy0 = s.y;
    const float sy1 = s.y + s.height;
    Strip acc;
    acc.y = s.y;
    acc.height = s.height;
    for (const Strip& m : clipMask) {
      const float my0 = m.y;
      const float my1 = m.y + m.height;
      if (my1 <= sy0 || my0 >= sy1) {
        continue;
      }
      for (const StripSpan& a : s.spans) {
        for (const StripSpan& b : m.spans) {
          const float x0 = std::max(a.x0, b.x0);
          const float x1 = std::min(a.x1, b.x1);
          addSpan(acc, x0, x1, a.coverage * b.coverage);
        }
      }
    }
    if (!acc.spans.empty()) {
      out.push_back(std::move(acc));
    }
  }
  strips = std::move(out);
}

void stripsToQuads(const std::vector<Strip>& strips, const Vec4& premul, std::vector<Quad>& quads) {
  for (const Strip& s : strips) {
    for (const StripSpan& sp : s.spans) {
      if (sp.x1 <= sp.x0 || s.height <= 0.f || sp.coverage <= 1e-4f) {
        continue;
      }
      Quad q;
      q.x = sp.x0;
      q.y = s.y;
      q.w = sp.x1 - sp.x0;
      q.h = s.height;
      q.r = premul.x * sp.coverage;
      q.g = premul.y * sp.coverage;
      q.b = premul.z * sp.coverage;
      q.a = premul.w * sp.coverage;
      quads.push_back(q);
    }
  }
}

void appendGradientQuad(
    std::vector<GradientQuad>& out,
    float x,
    float y,
    float w,
    float h,
    float coverage,
    const Matter& m
) {
  if (w <= 0.f || h <= 0.f || coverage <= 1e-4f || !m.isGradient() || m.gradientStopCount <= 0) {
    return;
  }
  GradientQuad q{};
  q.x = x;
  q.y = y;
  q.w = w;
  q.h = h;
  q.coverage = coverage;
  q.kind = m.gradientKind == GradientKind::Radial ? 1 : 0;
  q.p0x = m.gradientP0.x;
  q.p0y = m.gradientP0.y;
  q.p1x = m.gradientP1.x;
  q.p1y = m.gradientP1.y;
  q.radius = m.gradientRadius;
  q.stopCount = static_cast<std::uint8_t>(m.gradientStopCount);
  for (int i = 0; i < m.gradientStopCount; ++i) {
    const GradientStop& s = m.gradientStops[i];
    const Vec4 p = premulLinear(s.color);
    q.offsets[i] = s.offset;
    q.r[i] = p.x;
    q.g[i] = p.y;
    q.b[i] = p.z;
    q.a[i] = p.w;
  }
  out.push_back(q);
}

void stripsToGradientQuads(
    const std::vector<Strip>& strips, const Matter& m, std::vector<GradientQuad>& out
) {
  for (const Strip& s : strips) {
    for (const StripSpan& sp : s.spans) {
      if (sp.x1 <= sp.x0 || s.height <= 0.f) {
        continue;
      }
      appendGradientQuad(out, sp.x0, s.y, sp.x1 - sp.x0, s.height, sp.coverage, m);
    }
  }
}

namespace {

// Full-coverage axis rect as one strip; empty rects add nothing.
void solidRect(std::vector<Strip>& strips, float x, float y, float w, float h) {
  if (w <= 0.f || h <= 0.f) return;
  Strip s;
  s.y = y;
  s.height = h;
  addSpan(s, x, x + w, 1.f);
  strips.push_back(std::move(s));
}

// A chamfered stroke as outer minus inner chamfered rect.
struct ChamferRing {
  Rect outer, inner;
  float ocut, icut;
  bool hole;

  ChamferRing(const Rect& r, float cut, float width) {
    const float half = width * 0.5f;
    outer = {{r.origin.x - half, r.origin.y - half}, {r.size.x + width, r.size.y + width}};
    inner = {{r.origin.x + half, r.origin.y + half}, {r.size.x - width, r.size.y - width}};
    ocut = cut + half;
    icut = std::max(0.f, cut - half);
    hole = inner.size.x > 1e-4f && inner.size.y > 1e-4f;
  }
  float coverage(float px, float py) const {
    const float o = chamferCoverage(outer, ocut, px, py);
    if (!hole) return o;
    return std::max(0.f, o - chamferCoverage(inner, icut, px, py));
  }
};

// Clip, then emit as solid or gradient quads by the matter.
void emitStrips(
    std::vector<Strip>& strips,
    const ClipState& clip,
    const Matter& matter,
    std::vector<Quad>& quads,
    std::vector<GradientQuad>& gradients
) {
  clipStrips(strips, clip);
  if (matter.isGradient()) {
    stripsToGradientQuads(strips, matter, gradients);
  } else {
    stripsToQuads(strips, premulLinear(matter.color), quads);
  }
}

}  // namespace

void appendShape(
    std::vector<Quad>& quads,
    std::vector<BlitQuad>& blits,
    std::vector<GradientQuad>& gradients,
    const Shape& shape,
    const ClipState& clip
) {
  if (const auto* f = std::get_if<FillRect>(&shape)) {
    if (f->matter.isGradient()) {
      if (!clip.active) {
        appendGradientQuad(
            gradients,
            f->rect.origin.x,
            f->rect.origin.y,
            f->rect.size.x,
            f->rect.size.y,
            1.f,
            f->matter
        );
        return;
      }
      std::vector<Strip> strips;
      flattenRounded(f->rect, Radius{}, strips);
      clipStrips(strips, clip);
      stripsToGradientQuads(strips, f->matter, gradients);
      return;
    }
    if (!clip.active) {
      const Vec4 p = premulLinear(f->matter.color);
      Quad q;
      q.x = f->rect.origin.x;
      q.y = f->rect.origin.y;
      q.w = f->rect.size.x;
      q.h = f->rect.size.y;
      q.r = p.x;
      q.g = p.y;
      q.b = p.z;
      q.a = p.w;
      quads.push_back(q);
      return;
    }
    std::vector<Strip> strips;
    flattenRounded(f->rect, Radius{}, strips);
    clipStrips(strips, clip);
    stripsToQuads(strips, premulLinear(f->matter.color), quads);
  } else if (const auto* r = std::get_if<FillRounded>(&shape)) {
    if (r->matter.isGradient()) {
      std::vector<Strip> strips;
      flattenRounded(r->rect, r->radius, strips);
      clipStrips(strips, clip);
      stripsToGradientQuads(strips, r->matter, gradients);
      return;
    }
    std::vector<Strip> strips;
    flattenRounded(r->rect, r->radius, strips);
    clipStrips(strips, clip);
    stripsToQuads(strips, premulLinear(r->matter.color), quads);
  } else if (const auto* s = std::get_if<Stroke>(&shape)) {
    if (s->matter.isGradient()) {
      std::vector<Strip> strips;
      flattenStroke(s->rect, s->radius, s->width, strips);
      clipStrips(strips, clip);
      stripsToGradientQuads(strips, s->matter, gradients);
      return;
    }
    std::vector<Strip> strips;
    flattenStroke(s->rect, s->radius, s->width, strips);
    clipStrips(strips, clip);
    stripsToQuads(strips, premulLinear(s->matter.color), quads);
  } else if (const auto* ch = std::get_if<FillChamfer>(&shape)) {
    const float cut =
        std::min(std::max(0.f, ch->cut), std::min(ch->rect.size.x, ch->rect.size.y) * 0.5f);
    // Fast path: 3 solid interior rects (O(1) strips) + per-pixel
    // only in the 4 cut corner squares. Works with/without clip.
    if (cut > 1e-4f && ch->rect.size.x > 2.f * cut && ch->rect.size.y > 2.f * cut) {
      std::vector<Strip> strips;
      const float x0 = ch->rect.origin.x, y0 = ch->rect.origin.y;
      const float w = ch->rect.size.x, h = ch->rect.size.y;
      solidRect(strips, x0, y0 + cut, w, h - 2.f * cut);
      solidRect(strips, x0 + cut, y0, w - 2.f * cut, cut);
      solidRect(strips, x0 + cut, y0 + h - cut, w - 2.f * cut, cut);
      const float corners[4][2] = {
          {x0, y0}, {x0 + w - cut, y0}, {x0, y0 + h - cut}, {x0 + w - cut, y0 + h - cut}
      };
      for (int ci = 0; ci < 4; ++ci) {
        const float cx0 = corners[ci][0], cy0 = corners[ci][1];
        flattenCoverage(
            static_cast<int>(std::floor(cx0)) - 1,
            static_cast<int>(std::floor(cy0)) - 1,
            static_cast<int>(std::ceil(cx0 + cut)) + 1,
            static_cast<int>(std::ceil(cy0 + cut)) + 1,
            strips,
            [&](float px, float py) { return chamferCoverage(ch->rect, ch->cut, px, py); }
        );
      }
      emitStrips(strips, clip, ch->matter, quads, gradients);
      return;
    }
    auto flat = [&](std::vector<Strip>& strips) {
      const float x0 = std::floor(ch->rect.origin.x) - 1.f;
      const float y0 = std::floor(ch->rect.origin.y) - 1.f;
      const float x1 = std::ceil(ch->rect.origin.x + ch->rect.size.x) + 1.f;
      const float y1 = std::ceil(ch->rect.origin.y + ch->rect.size.y) + 1.f;
      flattenCoverage(
          static_cast<int>(x0),
          static_cast<int>(y0),
          static_cast<int>(x1),
          static_cast<int>(y1),
          strips,
          [&](float px, float py) { return chamferCoverage(ch->rect, cut, px, py); }
      );
    };
    std::vector<Strip> strips;
    flat(strips);
    emitStrips(strips, clip, ch->matter, quads, gradients);
  } else if (const auto* sc = std::get_if<StrokeChamfer>(&shape)) {
    const float half = sc->width * 0.5f;
    // Fast path: 4 axis edge rects (O(1) strips) + per-pixel only in
    // the 4 corner squares where the diagonals live.
    if (sc->width > 0.f && sc->rect.size.x > 0.f && sc->rect.size.y > 0.f) {
      const float x0 = sc->rect.origin.x, y0 = sc->rect.origin.y;
      const float w = sc->rect.size.x, h = sc->rect.size.y;
      const float cut = std::max(0.f, sc->cut);
      std::vector<Strip> strips;
      solidRect(strips, x0 + cut, y0 - half, w - 2.f * cut, sc->width);
      solidRect(strips, x0 + cut, y0 + h - half, w - 2.f * cut, sc->width);
      solidRect(strips, x0 - half, y0 + cut, sc->width, h - 2.f * cut);
      solidRect(strips, x0 + w - half, y0 + cut, sc->width, h - 2.f * cut);
      const ChamferRing ring(sc->rect, cut, sc->width);
      const float cs = cut + half + 2.f;
      const float corners[4][2] = {{x0, y0}, {x0 + w, y0}, {x0, y0 + h}, {x0 + w, y0 + h}};
      for (int ci = 0; ci < 4; ++ci) {
        const float ccx = corners[ci][0], ccy = corners[ci][1];
        flattenCoverage(
            static_cast<int>(std::floor(ccx - cs)),
            static_cast<int>(std::floor(ccy - cs)),
            static_cast<int>(std::ceil(ccx + cs)),
            static_cast<int>(std::ceil(ccy + cs)),
            strips,
            [&](float px, float py) { return ring.coverage(px, py); }
        );
      }
      emitStrips(strips, clip, sc->matter, quads, gradients);
      return;
    }
    auto flat = [&](std::vector<Strip>& strips) {
      const ChamferRing ring(sc->rect, sc->cut, sc->width);
      const Rect& outer = ring.outer;
      const int x0 = static_cast<int>(std::floor(outer.origin.x)) - 1;
      const int y0 = static_cast<int>(std::floor(outer.origin.y)) - 1;
      const int x1 = static_cast<int>(std::ceil(outer.origin.x + outer.size.x)) + 1;
      const int y1 = static_cast<int>(std::ceil(outer.origin.y + outer.size.y)) + 1;
      flattenCoverage(x0, y0, x1, y1, strips, [&](float px, float py) {
        return ring.coverage(px, py);
      });
    };
    std::vector<Strip> strips;
    flat(strips);
    emitStrips(strips, clip, sc->matter, quads, gradients);
  } else if (const auto* a = std::get_if<Arc>(&shape)) {
    auto flat = [&](std::vector<Strip>& strips) {
      const float o = a->radius + a->thickness * 0.5f + 1.f;
      const int x0 = static_cast<int>(std::floor(a->center.x - o));
      const int y0 = static_cast<int>(std::floor(a->center.y - o));
      const int x1 = static_cast<int>(std::ceil(a->center.x + o));
      const int y1 = static_cast<int>(std::ceil(a->center.y + o));
      // Coverage lives in the annulus radius +- (half + AA); rows only
      // sample its one or two column windows instead of the whole box.
      const float half = a->thickness * 0.5f;
      const float rout = a->radius + half + 1.f, rin = std::max(0.f, a->radius - half - 1.f);
      flattenCoverageRows(
          x0,
          y0,
          x1,
          y1,
          strips,
          [&](int y, ColumnRange* r) {
            const float dy = std::fabs(static_cast<float>(y) + 0.5f - a->center.y);
            if (dy > rout) return 0;
            const float xo = std::sqrt(rout * rout - dy * dy);
            const float xi = dy < rin ? std::sqrt(rin * rin - dy * dy) : 0.f;
            const float cx = a->center.x - 0.5f;
            const int la = static_cast<int>(std::floor(cx - xo)) - 1;
            const int lb = static_cast<int>(std::ceil(cx - xi)) + 2;
            const int ra = static_cast<int>(std::floor(cx + xi)) - 1;
            const int rb = static_cast<int>(std::ceil(cx + xo)) + 2;
            if (lb >= ra) {
              r[0] = {la, rb};
              return 1;
            }
            r[0] = {la, lb};
            r[1] = {ra, rb};
            return 2;
          },
          [&](float px, float py) { return arcCoverage(*a, px, py); }
      );
    };
    std::vector<Strip> strips;
    flat(strips);
    emitStrips(strips, clip, a->matter, quads, gradients);
  } else if (const auto* pl = std::get_if<Polyline>(&shape)) {
    auto flat = [&](std::vector<Strip>& strips) {
      if (pl->points.size() < 2) return;
      if (pl->dotPitch > 0.f) {
        // Dotted: stamp discs (O(dots × dot area), not O(bbox × dots)).
        flattenDottedPolyline(
            pl->points.data(), pl->points.size(), pl->width, pl->dotPitch, strips
        );
        return;
      }
      flattenSolidPolyline(pl->points.data(), pl->points.size(), pl->width, strips);
    };
    std::vector<Strip> strips;
    flat(strips);
    emitStrips(strips, clip, pl->matter, quads, gradients);
  } else if (const auto* b = std::get_if<Blit>(&shape)) {
    if (clip.active && (clip.rect.size.x <= 0.f || clip.rect.size.y <= 0.f)) {
      return;
    }
    blits.push_back(toBlitQuad(*b));
  } else if (const auto* run = std::get_if<GlyphRun>(&shape)) {
    if (clip.active && (clip.rect.size.x <= 0.f || clip.rect.size.y <= 0.f)) {
      return;
    }
    appendGlyphs(blits, *run);
  }
  // SlotHole: skip paint. Not dest-out.
}

void appendShapeWorld(
    std::vector<Quad>& quads,
    std::vector<BlitQuad>& blits,
    std::vector<GradientQuad>& gradients,
    const Shape& shape,
    const Mat4& world,
    const ClipState& clip
) {
  if (isAxisAligned(world)) {
    appendShape(quads, blits, gradients, transformShape(world, shape), clip);
    return;
  }
  // Rotated path: solid + gradient FillRect/Rounded/Chamfer,
  // Stroke/Chamfer, Arc, Polyline via inverse-mapped coverage.
  // Glyphs/blits fall back to AABB — safe, never garbage.
  if (std::get_if<Blit>(&shape) || std::get_if<GlyphRun>(&shape) || std::get_if<SlotHole>(&shape)) {
    appendShape(quads, blits, gradients, transformShape(world, shape), clip);
    return;
  }
  Mat4 inv = Mat4::identity();
  if (!invert2DAffine(world, inv)) {
    return;
  }
  // Local-space coverage closure per shape kind.
  const FillRounded* rr = std::get_if<FillRounded>(&shape);
  const FillRect* fr = std::get_if<FillRect>(&shape);
  const Stroke* st = std::get_if<Stroke>(&shape);
  const FillChamfer* fc = std::get_if<FillChamfer>(&shape);
  const StrokeChamfer* sc = std::get_if<StrokeChamfer>(&shape);
  const Arc* ar = std::get_if<Arc>(&shape);
  const Polyline* pl = std::get_if<Polyline>(&shape);
  Matter matter = Matter::solid(Color{255, 255, 255, 255});
  Rect aabb{};
  if (rr) {
    matter = rr->matter;
    aabb = transformRect(world, rr->rect);
  } else if (fr) {
    matter = fr->matter;
    aabb = transformRect(world, fr->rect);
  } else if (st) {
    matter = st->matter;
    const float o = std::max(0.f, st->width) * 0.5f;
    aabb = transformRect(
        world,
        {{st->rect.origin.x - o, st->rect.origin.y - o},
         {st->rect.size.x + o * 2.f, st->rect.size.y + o * 2.f}}
    );
  } else if (fc) {
    matter = fc->matter;
    aabb = transformRect(world, fc->rect);
  } else if (sc) {
    matter = sc->matter;
    const float o = std::max(0.f, sc->width) * 0.5f;
    aabb = transformRect(
        world,
        {{sc->rect.origin.x - o, sc->rect.origin.y - o},
         {sc->rect.size.x + o * 2.f, sc->rect.size.y + o * 2.f}}
    );
  } else if (ar) {
    matter = ar->matter;
    // Conservative AABB of full ring in world space.
    const float s =
        std::max(std::hypot(world.m[0], world.m[1]), std::hypot(world.m[4], world.m[5]));
    const float o = ar->radius * s + ar->thickness * s * 0.5f + 1.f;
    const Vec2 c = {
        world.m[0] * ar->center.x + world.m[4] * ar->center.y + world.m[12],
        world.m[1] * ar->center.x + world.m[5] * ar->center.y + world.m[13]
    };
    aabb = {{c.x - o, c.y - o}, {o * 2.f, o * 2.f}};
  } else if (pl) {
    matter = pl->matter;
    if (pl->points.empty()) return;
    float x0 = pl->points[0].x, y0 = pl->points[0].y, x1 = x0, y1 = y0;
    for (const Vec2& p : pl->points) {
      x0 = std::min(x0, p.x);
      y0 = std::min(y0, p.y);
      x1 = std::max(x1, p.x);
      y1 = std::max(y1, p.y);
    }
    const float pad = pl->width * 0.5f + 1.f;
    aabb = transformRect(world, {{x0 - pad, y0 - pad}, {x1 - x0 + pad * 2.f, y1 - y0 + pad * 2.f}});
  } else {
    appendShape(quads, blits, gradients, transformShape(world, shape), clip);
    return;
  }
  if (aabb.size.x <= 0.f || aabb.size.y <= 0.f) {
    return;
  }
  const int xi0 = static_cast<int>(std::floor(aabb.origin.x)) - 1;
  const int yi0 = static_cast<int>(std::floor(aabb.origin.y)) - 1;
  const int xi1 = static_cast<int>(std::ceil(aabb.origin.x + aabb.size.x)) + 1;
  const int yi1 = static_cast<int>(std::ceil(aabb.origin.y + aabb.size.y)) + 1;
  std::vector<Strip> strips;
  auto localPt = [&](float px, float py) -> Vec2 {
    const Vec4 v = inv * Vec4{px, py, 0, 1};
    return {v.x, v.y};
  };
  if (rr) {
    flattenCoverage(xi0, yi0, xi1, yi1, strips, [&](float px, float py) {
      const Vec2 l = localPt(px, py);
      return roundedCoverage(rr->rect, rr->radius, l.x, l.y);
    });
  } else if (fr) {
    flattenCoverage(xi0, yi0, xi1, yi1, strips, [&](float px, float py) {
      const Vec2 l = localPt(px, py);
      return roundedCoverage(fr->rect, Radius{}, l.x, l.y);
    });
  } else if (st) {
    const float half = st->width * 0.5f;
    const Rect outer{
        {st->rect.origin.x - half, st->rect.origin.y - half},
        {st->rect.size.x + st->width, st->rect.size.y + st->width}
    };
    Radius orad = st->radius;
    orad.lt += half;
    orad.rt += half;
    orad.lb += half;
    orad.rb += half;
    const Rect inner{
        {st->rect.origin.x + half, st->rect.origin.y + half},
        {st->rect.size.x - st->width, st->rect.size.y - st->width}
    };
    const bool hole = inner.size.x > 1e-4f && inner.size.y > 1e-4f;
    Radius irad{};
    if (hole) {
      irad.lt = std::max(0.f, st->radius.lt - half);
      irad.rt = std::max(0.f, st->radius.rt - half);
      irad.lb = std::max(0.f, st->radius.lb - half);
      irad.rb = std::max(0.f, st->radius.rb - half);
    }
    flattenCoverage(xi0, yi0, xi1, yi1, strips, [&](float px, float py) {
      const Vec2 l = localPt(px, py);
      const float o = roundedCoverage(outer, orad, l.x, l.y);
      if (!hole) return o;
      return std::max(0.f, o - roundedCoverage(inner, irad, l.x, l.y));
    });
  } else if (fc) {
    flattenCoverage(xi0, yi0, xi1, yi1, strips, [&](float px, float py) {
      const Vec2 l = localPt(px, py);
      return chamferCoverage(fc->rect, fc->cut, l.x, l.y);
    });
  } else if (sc) {
    const ChamferRing ring(sc->rect, sc->cut, sc->width);
    flattenCoverage(xi0, yi0, xi1, yi1, strips, [&](float px, float py) {
      const Vec2 l = localPt(px, py);
      return ring.coverage(l.x, l.y);
    });
  } else if (ar) {
    flattenCoverage(xi0, yi0, xi1, yi1, strips, [&](float px, float py) {
      const Vec2 l = localPt(px, py);
      return arcCoverage(*ar, l.x, l.y);
    });
  } else if (pl) {
    flattenCoverage(xi0, yi0, xi1, yi1, strips, [&](float px, float py) {
      const Vec2 l = localPt(px, py);
      return polylineCoverage(
          pl->points.data(), pl->points.size(), pl->width, pl->dotPitch, l.x, l.y
      );
    });
  }
  emitStrips(strips, clip, matter, quads, gradients);
  (void)blits;
  return;
}

}  // namespace glim::paint
