#pragma once

// 3D city backdrop: an extruded block city seen from an oblique pinhole
// camera, monochrome like the UI, with the orange route glowing along
// an avenue. Geometry (city, camera, visible faces) is plain values;
// paint only fills the projected faces.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Fui.h"
#include "MapMesh.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {
namespace city {

struct V3 {
  float x, y, z;
};
struct V2 {
  float x, y;
};

inline V3 operator-(V3 a, V3 b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline float dot(V3 a, V3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Ground is the z = 0 plane in meters; y runs away from the viewer.
struct Building {
  float x0, y0, x1, y1, h;
  float z0 = 0.f;     // base: setback tiers sit on the tier below
  float spire = 0.f;  // mast height above the roof
};

struct Layout {
  float block = 84.f;
  float street = 14.f;
  float avenue = 26.f;
  int avenueEvery = 4;
  // Dense cores placed where the cluster leaves the city visible: the
  // gap under the dial and the far skyline above it.
  V2 downtown{330.f, 240.f};
  V2 uptown{-60.f, 1350.f};
  // River across the far city: the default framing shows its near bank
  // under the top edge, larger windows reveal the far bank behind it.
  std::vector<V2> river{
      {-2600.f, 3700.f},
      {-1475.f, 2734.f},
      {-648.f, 2032.f},
      {51.f, 1743.f},
      {634.f, 1430.f},
      {1158.f, 961.f},
      {1705.f, 504.f},
      {2626.f, 71.f},
      {3900.f, -660.f}
  };
  float riverHalf = 105.f;
};

// Inclusive-exclusive block range [gx0, gx1) x [gy0, gy1).
struct GridSpan {
  int gx0, gx1, gy0, gy1;
};

// Street centerline coordinate of grid line i (avenues are wider).
inline float gridLine(const Layout& l, int i) {
  return static_cast<float>(i) * (l.block + l.street);
}
inline float gap(const Layout& l, int i) {
  return i % l.avenueEvery == 0 ? l.avenue : l.street;
}

// Parks span 2x2 blocks; their lots stay open ground with trees.
inline bool isPark(int gx, int gy) {
  return mapHash01(((gx >> 1) + 64) * 97 + ((gy >> 1) + 64) * 7 + 11) <
         0.07f;
}

// River centerline smoothed with Catmull-Rom, dense enough to read as a
// curve at any framing.
inline std::vector<V2> riverPath(const Layout& l) {
  const std::vector<V2>& k = l.river;
  std::vector<V2> out;
  for (std::size_t i = 0; i + 1 < k.size(); ++i) {
    const V2 p0 = k[i == 0 ? 0 : i - 1], p1 = k[i], p2 = k[i + 1],
             p3 = k[std::min(i + 2, k.size() - 1)];
    for (int s = 0; s < 12; ++s) {
      const float t = static_cast<float>(s) / 12.f, t2 = t * t,
                  t3 = t2 * t;
      auto cr = [&](float a, float b, float c, float d) {
        return 0.5f * (2.f * b + (c - a) * t +
                       (2.f * a - 5.f * b + 4.f * c - d) * t2 +
                       (3.f * b - a - 3.f * c + d) * t3);
      };
      out.push_back(
          {cr(p0.x, p1.x, p2.x, p3.x), cr(p0.y, p1.y, p2.y, p3.y)}
      );
    }
  }
  out.push_back(k.back());
  return out;
}

inline float distanceTo(const std::vector<V2>& line, V2 p) {
  float best = 1e9f;
  for (std::size_t i = 0; i + 1 < line.size(); ++i) {
    const V2 a = line[i], b = line[i + 1];
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float t = std::clamp(
        ((p.x - a.x) * dx + (p.y - a.y) * dy) / (dx * dx + dy * dy),
        0.f,
        1.f
    );
    best = std::min(
        best, std::hypot(p.x - a.x - dx * t, p.y - a.y - dy * t)
    );
  }
  return best;
}

// True when any corner of the rect lies within `pad` of the water.
inline bool touchesWater(
    const Layout& l,
    const std::vector<V2>& river,
    float x0,
    float y0,
    float x1,
    float y1,
    float pad
) {
  for (V2 p :
       {V2{x0, y0},
        V2{x1, y0},
        V2{x1, y1},
        V2{x0, y1},
        V2{(x0 + x1) * 0.5f, (y0 + y1) * 0.5f}})
    if (distanceTo(river, p) < l.riverHalf + pad) return true;
  return false;
}

inline std::vector<Building> buildCity(
    const Layout& l, const GridSpan& span
) {
  const std::vector<V2> river = riverPath(l);
  std::vector<Building> out;
  for (int gx = span.gx0; gx < span.gx1; ++gx) {
    for (int gy = span.gy0; gy < span.gy1; ++gy) {
      if (isPark(gx, gy)) continue;
      const int id = (gx + 64) * 131 + (gy + 64);
      const float bx0 = gridLine(l, gx) + gap(l, gx) * 0.5f;
      const float bx1 = gridLine(l, gx + 1) - gap(l, gx + 1) * 0.5f;
      const float by0 = gridLine(l, gy) + gap(l, gy) * 0.5f;
      const float by1 = gridLine(l, gy + 1) - gap(l, gy + 1) * 0.5f;
      const float cx = (bx0 + bx1) * 0.5f - l.downtown.x,
                  cy = (by0 + by1) * 0.5f - l.downtown.y;
      const float ux = cx + l.downtown.x - l.uptown.x,
                  uy = cy + l.downtown.y - l.uptown.y;
      const float core = std::max(
          std::exp(-(cx * cx + cy * cy) / (330.f * 330.f)),
          std::exp(-(ux * ux + uy * uy) / (520.f * 520.f))
      );
      // Downtown lots are big single towers; the outskirts subdivide
      // into low perimeter blocks with a courtyard gap.
      const int split =
          core > 0.45f
              ? 1
              : 2 + static_cast<int>(mapHash01(id * 7 + 2) * 1.99f);
      const int nx = core > 0.45f ? 1 : split,
                ny = core > 0.45f ? 1 + (mapHash01(id * 5 + 1) < 0.4f)
                                  : 2;
      for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < ny; ++j) {
          const int lot = id * 17 + i * 5 + j;
          if (core < 0.2f && mapHash01(lot * 23 + 9) < 0.12f)
            continue;  // empty lot
          const float inset = 2.f + mapHash01(lot * 13 + 3) * 3.f;
          const float w = (bx1 - bx0) / static_cast<float>(nx),
                      d = (by1 - by0) / static_cast<float>(ny);
          const float r = mapHash01(lot * 19 + 7);
          const float h =
              6.f + r * 16.f + core * (30.f + 300.f * r * r * r);
          Building t{
              bx0 + w * static_cast<float>(i) + inset,
              by0 + d * static_cast<float>(j) + inset,
              bx0 + w * static_cast<float>(i + 1) - inset,
              by0 + d * static_cast<float>(j + 1) - inset,
              h
          };
          if (touchesWater(l, river, t.x0, t.y0, t.x1, t.y1, 14.f))
            continue;
          // Towers step back: a podium, then narrower shafts on top.
          const int tiers = h > 140.f ? 3 : h > 70.f ? 2 : 1;
          float base = 0.f;
          for (int k = 0; k < tiers; ++k) {
            const float top =
                k + 1 == tiers
                    ? h
                    : base + (h - base) * (k == 0 ? 0.3f : 0.55f);
            const bool crown = k + 1 == tiers;
            const float spire =
                crown && h > 170.f && mapHash01(lot * 29 + 5) < 0.55f
                    ? 18.f + mapHash01(lot * 31 + 1) * 46.f
                    : 0.f;
            out.push_back({t.x0, t.y0, t.x1, t.y1, top, base, spire});
            const float sx = (t.x1 - t.x0) * 0.14f,
                        sy = (t.y1 - t.y0) * 0.14f;
            t = {t.x0 + sx, t.y0 + sy, t.x1 - sx, t.y1 - sy, h};
            base = top;
          }
          // Mid-rise roofs carry a plant room, concentric so it sorts
          // straight after its roof.
          if (tiers == 1 && h > 16.f &&
              mapHash01(lot * 37 + 3) < 0.6f) {
            const float sx = (t.x1 - t.x0) * 0.32f,
                        sy = (t.y1 - t.y0) * 0.32f;
            out.push_back(
                {t.x0 + sx,
                 t.y0 + sy,
                 t.x1 - sx,
                 t.y1 - sy,
                 h + 3.f + mapHash01(lot * 41) * 5.f,
                 h}
            );
          }
        }
      }
    }
  }
  return out;
}

// Pinhole camera orbiting a ground target; focal scales with the
// viewport height so the city reframes with the window.
struct Camera {
  V3 eye, right, up, fwd;
  float focal, cx, cy;
};

inline Camera orbit(
    const Box& box, V3 target, float yawDeg, float pitchDeg, float dist
) {
  const float yaw = yawDeg * 3.14159265f / 180.f,
              pitch = pitchDeg * 3.14159265f / 180.f;
  const V3 fwd{
      std::sin(yaw) * std::cos(pitch),
      std::cos(yaw) * std::cos(pitch),
      -std::sin(pitch)
  };
  const V3 eye{
      target.x - fwd.x * dist,
      target.y - fwd.y * dist,
      target.z - fwd.z * dist
  };
  const V3 right{std::cos(yaw), -std::sin(yaw), 0.f};
  const V3 up{
      right.y * fwd.z - right.z * fwd.y,
      right.z * fwd.x - right.x * fwd.z,
      right.x * fwd.y - right.y * fwd.x
  };
  return {
      eye,
      right,
      up,
      fwd,
      box.h * 1.25f,
      box.x + box.w * 0.5f,
      box.y + box.h * 0.5f
  };
}

// False when the point is behind or too close to the eye.
inline bool project(const Camera& c, V3 p, V2* out) {
  const V3 v = p - c.eye;
  const float z = dot(v, c.fwd);
  if (z < 1.f) return false;
  out->x = c.cx + c.focal * dot(v, c.right) / z;
  out->y = c.cy - c.focal * dot(v, c.up) / z;
  return true;
}

inline float depth(const Camera& c, V3 p) {
  return dot(p - c.eye, c.fwd);
}

// Ground (z = 0) point under screen point `s`; false above the horizon.
inline bool unprojectGround(const Camera& c, V2 s, V2* out) {
  const float u = (s.x - c.cx) / c.focal, v = (c.cy - s.y) / c.focal;
  const V3 d{
      c.fwd.x + c.right.x * u + c.up.x * v,
      c.fwd.y + c.right.y * u + c.up.y * v,
      c.fwd.z + c.right.z * u + c.up.z * v
  };
  if (d.z > -1e-4f) return false;
  const float t = -c.eye.z / d.z;
  *out = {c.eye.x + d.x * t, c.eye.y + d.y * t};
  return true;
}

// Blocks whose ground the screen box sees, plus a margin for towers
// standing just below the frame and rising into it.
inline GridSpan groundSpan(
    const Layout& l, const Camera& c, const Box& box
) {
  float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
  for (V2 s :
       {V2{box.x, box.y},
        V2{box.x + box.w, box.y},
        V2{box.x, box.y + box.h},
        V2{box.x + box.w, box.y + box.h}}) {
    V2 g{0.f, 0.f};
    if (!unprojectGround(c, s, &g)) continue;
    x0 = std::min(x0, g.x), x1 = std::max(x1, g.x),
    y0 = std::min(y0, g.y), y1 = std::max(y1, g.y);
  }
  const float pitch = l.block + l.street;
  return {
      static_cast<int>(std::floor(x0 / pitch)) - 2,
      static_cast<int>(std::ceil(x1 / pitch)) + 2,
      static_cast<int>(std::floor(y0 / pitch)) - 3,
      static_cast<int>(std::ceil(y1 / pitch)) + 2
  };
}

struct Face {
  std::array<V2, 4> pts;
  float shade;  // 0 = unlit side, 1 = roof
};

// Visible faces of one building, back to front: camera-facing walls
// then the roof. Empty when any corner falls behind the eye.
inline std::vector<Face> visibleFaces(
    const Camera& c, const Building& b
) {
  const V3 base[4] = {
      {b.x0, b.y0, b.z0},
      {b.x1, b.y0, b.z0},
      {b.x1, b.y1, b.z0},
      {b.x0, b.y1, b.z0}
  };
  V2 lo[4], hi[4];
  for (int i = 0; i < 4; ++i) {
    if (!project(c, base[i], &lo[i]) ||
        !project(c, {base[i].x, base[i].y, b.h}, &hi[i]))
      return {};
  }
  // Wall i joins corner i to i+1; outward normals: -y, +x, +y, -x.
  // Light comes from the upper left, so -x and +y walls stay dark.
  const V3 normals[4] = {{0, -1, 0}, {1, 0, 0}, {0, 1, 0}, {-1, 0, 0}};
  const float lit[4] = {0.55f, 0.75f, 0.3f, 0.2f};
  std::vector<Face> faces;
  for (int i = 0; i < 4; ++i) {
    const int k = (i + 1) % 4;
    const V3 mid{
        (base[i].x + base[k].x) * 0.5f,
        (base[i].y + base[k].y) * 0.5f,
        (b.z0 + b.h) * 0.5f
    };
    if (dot(normals[i], c.eye - mid) <= 0.f) continue;
    faces.push_back({{lo[i], lo[k], hi[k], hi[i]}, lit[i]});
  }
  faces.push_back({{hi[0], hi[1], hi[2], hi[3]}, 1.f});
  return faces;
}

// Route along avenues, ground space, in travel order: up the right side
// of the frame (as in the reference), clear of the dial.
inline std::vector<V3> routePath(const Layout& l) {
  auto at = [&](int gx, int gy) {
    return V3{gridLine(l, gx), gridLine(l, gy), 0.5f};
  };
  return {
      at(1, -3), at(1, 1), at(6, 1), at(6, 10), at(9, 10), at(9, 20)
  };
}

}  // namespace city

}  // namespace toolkit
}  // namespace glim
