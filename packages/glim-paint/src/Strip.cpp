#include "Strip.h"

#include <glim/paint/ColorSpace.h>

#include <algorithm>
#include <cmath>

#include "StripRows.h"

namespace glim::paint {
namespace {

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
    std::vector<Strip> strips;
    flattenPolyline(*pl, strips);
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
