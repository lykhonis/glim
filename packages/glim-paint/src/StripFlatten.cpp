#include <glim/paint/ColorSpace.h>

#include <algorithm>
#include <cmath>

#include "Strip.h"
#include "StripRows.h"

namespace glim::paint {
namespace {

inline float discCoverage(float cx, float cy, float r, float px, float py) {
  const float aa = 0.5f - (std::hypot(px - cx, py - cy) - r);
  if (aa <= 0.f) return 0.f;
  if (aa >= 1.f) return 1.f;
  return aa;
}

}  // namespace

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
      float t0 = 0.f, t1 = 1.f;
      const float dy = b.y - a.y;
      if (std::fabs(dy) > 1e-6f) {
        const float ta = (cy - pad - a.y) / dy, tb = (cy + pad - a.y) / dy;
        t0 = std::max(0.f, std::min(ta, tb));
        t1 = std::min(1.f, std::max(ta, tb));
      }
      const float xa = a.x + (b.x - a.x) * t0, xb = a.x + (b.x - a.x) * t1;
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
            if (x < r.lo || x > r.hi) continue;
            float t = 0.f, len = 0.f;
            best = std::min(best, segDist(p, pts[r.segment], pts[r.segment + 1], t, len));
          }
          cov = snapCoverage(std::clamp(0.5f - (best - half), 0.f, 1.f));
        }
        if (x == lo) {
          runCov = cov;
          continue;
        }
        if (x < hi && sameCoverage(cov, runCov)) continue;
        if (runCov > 0.f) addSpan(s, static_cast<float>(runX), static_cast<float>(x), runCov);
        runX = x;
        runCov = cov;
      }
    }
    if (!s.spans.empty()) out.push_back(std::move(s));
  }
  packHeight1(out);
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

}  // namespace glim::paint
