#pragma once

// Shared procedural map backdrop (paQys.jpg): cracked-street mesh +
// waypoint triangles + diagonal arterials + dotted route + vignette.
// Single home for the subtle-cluster pixels: Map3D's foreign-texture
// fallback paints through here. No foreign surfaces.

#include <cstdint>

#include "../InfoTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

// Deterministic pseudo-random 0..1 (no RNG state in widget).
inline float mapHash01(int n) {
  unsigned x = static_cast<unsigned>(n * 2654435761u);
  x ^= x >> 13;
  x *= 1274126177u;
  x ^= x >> 16;
  return static_cast<float>(x % 1000) / 1000.f;
}

inline void paintSubtleMapRoute(
  const Box& box, AnySink& sink, const Engine& e
);
inline void paintMapCar(
  const Box& box, AnySink& sink, const Engine& e, float u, float v
);
inline void
paintSubtleMapVignette(const Box& box, AnySink& sink, const Engine&);

inline void paintSubtleMap(
  const Box& box, AnySink& sink, const Engine& e
) {
  const std::uint32_t bgTop =
    themeColor(e, kInfoCardTop, Info::kCardTop);
  const std::uint32_t bgBottom =
    themeColor(e, kInfoCardBottom, Info::kCardBottom);
  const std::uint32_t ink = themeColor(e, kThemeInkOnLight, Info::kInk);

  sink.fillGradient(box, 8, bgTop, bgBottom);

  // Street ink: reference streets read as dim blue-gray (~#3a4552 on
  // near-black), not the near-transparent card grid. kGrid (0x14) is
  // for card interiors; the map uses its own ink so gutters read.
  const std::uint32_t street = 0x8fa0b5ff;
  // Cracked-street mesh (reference paQys.jpg: irregular broken
  // streets, never full-length graph-paper lines). Hashed verticals
  // + horizontals chopped into segments with gaps and per-segment
  // x/y jitter. Axis fills stay O(1) quads each — no long-polyline
  // bbox scans (see Strip::flattenCoverage cost).
  const float meshA = 0.30f;
  {
    // Short broken runs (~70px) with wide jitter + per-run brightness
    // so the eye can't connect them into graph-paper lines.
    const int nVert = 26;
    for (int i = 0; i < nVert; ++i) {
      float x = box.x + 10.f + mapHash01(i * 3 + 11) * (box.w - 20.f);
      const int segs = 9 + (i % 3);
      for (int sg = 0; sg < segs; ++sg) {
        float t0 = static_cast<float>(sg) / static_cast<float>(segs);
        float t1 =
          static_cast<float>(sg + 1) / static_cast<float>(segs);
        float gap = 0.25f + mapHash01(i * 13 + sg * 7 + 2) * 0.40f;
        float ya = box.y + 6.f + (box.h - 12.f) * t0;
        float yb =
          box.y + 6.f + (box.h - 12.f) * (t0 + (t1 - t0) * (1.f - gap));
        float jx = (mapHash01(i * 5 + sg * 11 + 4) - 0.5f) * 20.f;
        float ba = meshA * (0.5f + mapHash01(i * 7 + sg * 3 + 9));
        if (yb - ya > 7.f)
          sink.fill({x + jx, ya, 1.f, yb - ya}, withAlpha(street, ba));
      }
    }
    const int nHoriz = 9;
    for (int i = 0; i < nHoriz; ++i) {
      float y = box.y + 10.f + mapHash01(i * 7 + 21) * (box.h - 20.f);
      const int segs = 10 + (i % 2);
      for (int sg = 0; sg < segs; ++sg) {
        float t0 = static_cast<float>(sg) / static_cast<float>(segs);
        float t1 =
          static_cast<float>(sg + 1) / static_cast<float>(segs);
        float gap = 0.30f + mapHash01(i * 17 + sg * 5 + 6) * 0.40f;
        float xa = box.x + 6.f + (box.w - 12.f) * t0;
        float xb =
          box.x + 6.f + (box.w - 12.f) * (t0 + (t1 - t0) * (1.f - gap));
        float jy = (mapHash01(i * 9 + sg * 13 + 8) - 0.5f) * 12.f;
        float ba = meshA * (0.5f + mapHash01(i * 3 + sg * 7 + 1));
        if (xb - xa > 10.f)
          sink.fill({xa, y + jy, xb - xa, 1.f}, withAlpha(street, ba));
      }
    }
  }
  // Irregular short streets: deterministic angled-ish shorts
  // (axis boxes of varying length — no rotation primitive).
  {
    int nShorts = 12;
    for (int i = 0; i < nShorts; ++i) {
      float hx = mapHash01(i * 2 + 1), hy = mapHash01(i * 2 + 7);
      float hl = mapHash01(i * 3 + 13);
      float x = box.x + 8.f + hx * (box.w - 16.f);
      float y = box.y + 8.f + hy * (box.h - 16.f);
      float len = 14.f + hl * 44.f;
      bool horiz = (i % 3) != 2;
      if (horiz) {
        if (x + len > box.x + box.w - 6.f)
          len = box.x + box.w - 6.f - x;
        if (len > 4.f)
          sink.fill({x, y, len, 1.f}, withAlpha(street, 0.30f));
      } else {
        if (y + len > box.y + box.h - 6.f)
          len = box.y + box.h - 6.f - y;
        if (len > 4.f)
          sink.fill({x, y, 1.f, len}, withAlpha(street, 0.30f));
      }
    }
  }
  // Waypoint triangles like the reference map: a few small
  // upward markers scattered on the mesh.
  {
    const float tx[4] = {0.30f, 0.62f, 0.45f, 0.75f};
    const float ty[4] = {0.18f, 0.33f, 0.60f, 0.72f};
    for (int i = 0; i < 4; ++i) {
      float x = box.x + tx[i] * box.w;
      float y = box.y + ty[i] * box.h;
      sink.fill({x - 5.f, y, 10.f, 2.f}, withAlpha(ink, 0.28f));
      sink.fill({x - 3.f, y - 3.f, 6.f, 2.f}, withAlpha(ink, 0.28f));
      sink.fill({x - 1.f, y - 6.f, 2.f, 2.f}, withAlpha(ink, 0.28f));
    }
  }
  // Diagonal arterials: true rotated bars (compositor-native).
  for (int i = 0; i < 14; ++i) {
    float hx = mapHash01(i * 2 + 101), hy = mapHash01(i * 2 + 107);
    float hl = mapHash01(i * 3 + 113);
    float x = box.x + 8.f + hx * (box.w - 16.f);
    float y = box.y + 8.f + hy * (box.h - 16.f);
    float len = 16.f + hl * 30.f;
    float deg = (i % 2 == 0) ? (18.f + mapHash01(i * 5 + 131) * 22.f)
                             : -(20.f + mapHash01(i * 5 + 137) * 24.f);
    sink.pushRotate(deg, x, y);
    sink.fill(
      {x - len * 0.5f, y - 0.5f, len, 1.f}, withAlpha(street, 0.28f)
    );
    sink.pop();
  }
  // Reference cluster: thin dotted orange route zigzagging down the
  // right side behind the gauge cards (compositor Polyline).
  paintSubtleMapRoute(box, sink, e);
  // Ego vehicle on the route (visible gutter below the cards): halo +
  // white chevron + accent core. Pure 2D ops, same on every backend.
  paintMapCar(box, sink, e, 0.64f, 0.88f);
  // Vignette: darken top/bottom so cards sit in depth.
  paintSubtleMapVignette(box, sink, e);
}

// Route overlay, shared by the fallback mesh and the foreign 3D layer
// (stays Glim-side: crisp AA, theme accent, zero re-tessellation cost).
inline void paintSubtleMapRoute(
  const Box& box, AnySink& sink, const Engine& e
) {
  const std::uint32_t accent =
    themeColor(e, kInfoAccent, Info::kAccent);
  const float rx[8] = {
    0.66f, 0.70f, 0.67f, 0.71f, 0.68f, 0.72f, 0.66f, 0.62f
  };
  const float ry[8] = {
    0.06f, 0.18f, 0.30f, 0.42f, 0.54f, 0.66f, 0.80f, 0.94f
  };
  float route[16];
  for (int i = 0; i < 8; ++i) {
    route[i * 2] = box.x + rx[i] * box.w;
    route[i * 2 + 1] = box.y + ry[i] * box.h;
  }
  sink.pushBlend(BlendMode::Plus);
  sink.polyline(route, 8, 2.5f, 6.f, accent);
  sink.pop();
}

// Ego vehicle marker on the route: Plus halo + white navigation
// chevron + accent core. (u,v) are box fractions; default sits on the
// route in the visible bottom gutter. Heading points up-route.
inline void paintMapCar(
  const Box& box, AnySink& sink, const Engine& e, float u, float v
) {
  const std::uint32_t accent =
    themeColor(e, kInfoAccent, Info::kAccent);
  const float x = box.x + u * box.w;
  const float y = box.y + v * box.h;
  // White navigation chevron (paper-plane) + accent core. No halo
  // quad: the halo rasterized as a solid block at this size.
  float chev[10] = {
    x,
    y - 8.f,
    x + 7.f,
    y + 6.f,
    x,
    y + 2.f,
    x - 7.f,
    y + 6.f,
    x,
    y - 8.f
  };
  sink.polyline(chev, 5, 2.f, 0.f, 0xffffffffu);
  sink.fillRounded({x - 2.f, y - 2.f, 4.f, 4.f}, 2, accent);
}

inline void
paintSubtleMapVignette(const Box& box, AnySink& sink, const Engine&) {
  sink.fillGradient(
    {box.x, box.y, box.w, 120.f},
    0,
    withAlpha(0x000000ff, 0.35f),
    0x00000000u
  );
  sink.fillGradient(
    {box.x, box.y + box.h - 120.f, box.w, 120.f},
    0,
    0x00000000u,
    withAlpha(0x000000ff, 0.35f)
  );
}

}  // namespace toolkit
}  // namespace glim
