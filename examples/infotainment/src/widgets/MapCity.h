#pragma once

// 3D city backdrop: an extruded block city seen from an oblique pinhole
// camera, monochrome like the UI, with the orange route glowing along
// an avenue. Geometry (city, camera, visible faces) is plain values;
// paint only fills the projected faces.
//
// Glim paint has no polygon shape, so convex faces are filled with a
// one-pixel horizontal serpentine polyline: polyline coverage is the
// union of its segments, so rows at pixel pitch with unit width are a
// solid, anti-aliased fill on every backend (CPU and GPU share the
// strip path).

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

// ---- Paint ----

// Convex polygon fill as a one-pixel horizontal serpentine (see
// header).
inline void fillConvex(
    AnySink& sink, const city::V2* pts, int n, std::uint32_t rgba
) {
  float top = pts[0].y, bottom = pts[0].y;
  for (int i = 1; i < n; ++i)
    top = std::min(top, pts[i].y), bottom = std::max(bottom, pts[i].y);
  if (bottom - top < 0.25f) return;
  auto span = [&](float y, float* xl, float* xr) {
    *xl = 1e9f;
    *xr = -1e9f;
    for (int i = 0; i < n; ++i) {
      const city::V2 a = pts[i], b = pts[(i + 1) % n];
      if ((y < std::min(a.y, b.y)) || (y > std::max(a.y, b.y)) ||
          a.y == b.y)
        continue;
      const float x = a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y);
      *xl = std::min(*xl, x);
      *xr = std::max(*xr, x);
    }
    return *xl <= *xr;
  };
  std::vector<float> zig;
  bool forward = true;
  auto row = [&](float y) {
    float xl, xr;
    if (!span(std::clamp(y, top + 1e-3f, bottom - 1e-3f), &xl, &xr))
      return;
    xl += 0.5f;
    xr -= 0.5f;
    if (xr < xl) xl = xr = (xl + xr) * 0.5f;
    zig.insert(
        zig.end(),
        forward ? std::initializer_list<float>{xl, y, xr, y}
                : std::initializer_list<float>{xr, y, xl, y}
    );
    forward = !forward;
  };
  const float first = top + 0.5f, last = bottom - 0.5f;
  if (last <= first) {
    row((top + bottom) * 0.5f);
  } else {
    row(first);
    for (float y = std::floor(first) + 1.5f; y < last; y += 1.f) row(y);
    row(last);
  }
  if (zig.size() == 2) zig.insert(zig.end(), {zig[0] + 0.01f, zig[1]});
  if (zig.size() >= 4)
    sink.polyline(zig.data(), zig.size() / 2, 1.f, 0.f, rgba);
}

inline std::uint32_t cityGray(float v, float fog) {
  // Monochrome ramp from the ground toward lit rims, hazed with
  // distance.
  const float l = 10.f + 72.f * v * (1.f - fog);
  const auto c = static_cast<std::uint32_t>(std::clamp(l, 0.f, 255.f));
  const auto b =
      static_cast<std::uint32_t>(std::clamp(l * 1.06f + 1.f, 0.f, 255.f)
      );
  return (c << 24) | (c << 16) | (b << 8) | 0xffu;
}

// Flat arrow lying on the ground at `at`, pointing along `dir` (unit).
inline void paintGroundChevron(
    AnySink& sink,
    const city::Camera& cam,
    city::V3 at,
    city::V2 dir,
    float size
) {
  using namespace city;
  const V2 n{-dir.y, dir.x};
  const V3 w[4] = {
      {at.x + dir.x * size, at.y + dir.y * size, at.z},
      {at.x - dir.x * size * 0.6f + n.x * size * 0.8f,
       at.y - dir.y * size * 0.6f + n.y * size * 0.8f,
       at.z},
      {at.x - dir.x * size * 0.15f, at.y - dir.y * size * 0.15f, at.z},
      {at.x - dir.x * size * 0.6f - n.x * size * 0.8f,
       at.y - dir.y * size * 0.6f - n.y * size * 0.8f,
       at.z}
  };
  V2 p[4];
  for (int i = 0; i < 4; ++i)
    if (!project(cam, w[i], &p[i])) return;
  const std::vector<float> outline{
      p[0].x,
      p[0].y,
      p[1].x,
      p[1].y,
      p[2].x,
      p[2].y,
      p[3].x,
      p[3].y,
      p[0].x,
      p[0].y
  };
  sink.pushBlend(BlendMode::Plus);
  sink.polyline(outline.data(), 5, 7.f, 0.f, 0xfa78082cu);
  sink.pop();
  // Two convex halves of the dart.
  const V2 left[3] = {p[0], p[1], p[2]}, right[3] = {p[0], p[2], p[3]};
  fillConvex(sink, left, 3, 0xfa7808ffu);
  fillConvex(sink, right, 3, 0xfa7808ffu);
}

// The city is framed by the design box (1168x784) centered in the
// window and scaled up only once the window outgrows the baked plate,
// so its look holds while resizing and one bake covers every size.
struct CityView {
  Box plate;   // baked ground + buildings, window px
  Box camera;  // box the camera frames
};

inline constexpr float kCityPlateW = 2048.f, kCityPlateH = 1152.f;

inline CityView cityView(const Box& box) {
  const float s =
      std::max({1.f, box.w / kCityPlateW, box.h / kCityPlateH});
  const float cx = box.x + box.w * 0.5f, cy = box.y + box.h * 0.5f;
  const float pw = kCityPlateW * s, ph = kCityPlateH * s;
  const float cw = 1168.f * s, ch = 784.f * s;
  return {
      {std::round(cx - pw * 0.5f), std::round(cy - ph * 0.5f), pw, ph},
      {std::round(cx - pw * 0.5f) + (pw - cw) * 0.5f,
       std::round(cy - ph * 0.5f) + (ph - ch) * 0.5f,
       cw,
       ch}
  };
}

inline city::Camera cityCamera(const CityView& v) {
  return city::orbit(v.camera, {160.f, 700.f, 0.f}, 33.f, 54.f, 1750.f);
}

// Static layer: ground, water, curbs, roads and buildings over `bounds`
// (the plate). The city is generated over exactly the ground the plate
// sees, so it reaches every edge at any framing.
inline void paintCityGround(
    const Box& bounds, AnySink& sink, const city::Camera& cam
) {
  using namespace city;
  static const Layout layout;
  const GridSpan span = groundSpan(layout, cam, bounds);
  const std::vector<Building> buildings = buildCity(layout, span);
  const std::vector<V2> river = riverPath(layout);
  const float nearD = 1100.f, farD = 3600.f;
  auto fogAt = [&](V3 p) {
    return std::clamp(
        (depth(cam, p) - nearD) / (farD - nearD), 0.f, 0.85f
    );
  };
  const Box& box = bounds;
  auto onPlate = [&](const V2* p, int n, float pad) {
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (int i = 0; i < n; ++i)
      minX = std::min(minX, p[i].x), maxX = std::max(maxX, p[i].x),
      minY = std::min(minY, p[i].y), maxY = std::max(maxY, p[i].y);
    return maxX >= box.x - pad && minX <= box.x + box.w + pad &&
           maxY >= box.y - pad && minY <= box.y + box.h + pad;
  };

  sink.fill(box, 0x08090bff);
  // Curbs: every dry block outline on the ground, fading with distance.
  for (int gx = span.gx0; gx < span.gx1; ++gx) {
    for (int gy = span.gy0; gy < span.gy1; ++gy) {
      const float x0 = gridLine(layout, gx) + gap(layout, gx) * 0.5f;
      const float x1 =
          gridLine(layout, gx + 1) - gap(layout, gx + 1) * 0.5f;
      const float y0 = gridLine(layout, gy) + gap(layout, gy) * 0.5f;
      const float y1 =
          gridLine(layout, gy + 1) - gap(layout, gy + 1) * 0.5f;
      V2 p[4];
      const V3 w[4] = {
          {x0, y0, 0}, {x1, y0, 0}, {x1, y1, 0}, {x0, y1, 0}
      };
      bool ok = true;
      for (int i = 0; i < 4; ++i) ok = ok && project(cam, w[i], &p[i]);
      if (!ok || !onPlate(p, 4, 2.f) ||
          touchesWater(layout, river, x0, y0, x1, y1, 4.f))
        continue;
      const float fog =
          fogAt({(x0 + x1) * 0.5f, (y0 + y1) * 0.5f, 0.f});
      if (isPark(gx, gy)) {
        fillConvex(sink, p, 4, cityGray(0.16f, fog));
        // Tree canopy dots on a jittered grid.
        for (float tx = x0 + 8.f; tx < x1 - 4.f; tx += 13.f)
          for (float ty = y0 + 8.f; ty < y1 - 4.f; ty += 13.f) {
            const int tid = static_cast<int>(tx * 7.f + ty * 13.f);
            V2 q;
            if (mapHash01(tid) < 0.3f ||
                !project(
                    cam, {tx + mapHash01(tid + 1) * 5.f, ty, 4.f}, &q
                ))
              continue;
            sink.fillRounded(
                {q.x - 2.f, q.y - 1.5f, 4.f, 3.f},
                1,
                cityGray(0.42f, fog)
            );
          }
      }
      fui::path(
          sink,
          {p[0].x,
           p[0].y,
           p[1].x,
           p[1].y,
           p[2].x,
           p[2].y,
           p[3].x,
           p[3].y,
           p[0].x,
           p[0].y},
          1.f,
          cityGray(0.5f, fog)
      );
    }
  }

  // River: still dark water between lit embankments, with faint current
  // lines along the flow.
  auto bank = [&](float offset) {
    std::vector<V2> out;
    for (std::size_t i = 0; i < river.size(); ++i) {
      const V2 a = river[i == 0 ? 0 : i - 1],
               b = river[std::min(i + 1, river.size() - 1)];
      const float len = std::hypot(b.x - a.x, b.y - a.y);
      out.push_back(
          {river[i].x - (b.y - a.y) / len * offset,
           river[i].y + (b.x - a.x) / len * offset}
      );
    }
    return out;
  };
  auto groundLine =
      [&](const std::vector<V2>& line, float w, float dash, float shade
      ) {
        for (std::size_t i = 0; i + 1 < line.size(); ++i) {
          V2 pa, pb;
          if (!project(cam, {line[i].x, line[i].y, 0.f}, &pa) ||
              !project(cam, {line[i + 1].x, line[i + 1].y, 0.f}, &pb))
            continue;
          const V2 seg[2] = {pa, pb};
          if (!onPlate(seg, 2, 4.f)) continue;
          const float fog = fogAt({line[i].x, line[i].y, 0.f});
          sink.polyline(
              std::vector<float>{pa.x, pa.y, pb.x, pb.y}.data(),
              2,
              w,
              dash,
              cityGray(shade, fog)
          );
        }
      };
  {
    const std::vector<V2> l = bank(-layout.riverHalf),
                          r = bank(layout.riverHalf);
    for (std::size_t i = 0; i + 1 < river.size(); ++i) {
      V2 q[4];
      if (!project(cam, {l[i].x, l[i].y, 0.f}, &q[0]) ||
          !project(cam, {l[i + 1].x, l[i + 1].y, 0.f}, &q[1]) ||
          !project(cam, {r[i + 1].x, r[i + 1].y, 0.f}, &q[2]) ||
          !project(cam, {r[i].x, r[i].y, 0.f}, &q[3]))
        continue;
      if (onPlate(q, 4, 2.f)) fillConvex(sink, q, 4, 0x040506ffu);
    }
    for (float o : {-0.55f, -0.2f, 0.3f, 0.62f})
      groundLine(bank(layout.riverHalf * o), 1.f, 9.f, 0.2f);
    for (float o : {-1.f, 1.f}) {
      groundLine(bank(layout.riverHalf * o), 1.5f, 0.f, 1.15f);
      groundLine(bank((layout.riverHalf + 9.f) * o), 1.f, 0.f, 0.55f);
    }
  }

  // Avenues: lit lane edges and a dashed median, like a GIS road layer.
  // Over the river they become bridges: a deck, then the same lanes.
  auto road = [&](V3 a, V3 b, float w, std::uint32_t c, float dash) {
    V2 pa, pb;
    if (!project(cam, a, &pa) || !project(cam, b, &pb)) return;
    const V2 seg[2] = {pa, pb};
    if (!onPlate(seg, 2, 4.f)) return;
    sink.polyline(
        std::vector<float>{pa.x, pa.y, pb.x, pb.y}.data(), 2, w, dash, c
    );
  };
  auto deck = [&](V2 a, V2 b, float half) {
    const float len = std::hypot(b.x - a.x, b.y - a.y),
                nx = -(b.y - a.y) / len * half,
                ny = (b.x - a.x) / len * half;
    V2 q[4];
    if (!project(cam, {a.x + nx, a.y + ny, 0.f}, &q[0]) ||
        !project(cam, {b.x + nx, b.y + ny, 0.f}, &q[1]) ||
        !project(cam, {b.x - nx, b.y - ny, 0.f}, &q[2]) ||
        !project(cam, {a.x - nx, a.y - ny, 0.f}, &q[3]))
      return;
    if (!onPlate(q, 4, 2.f)) return;
    const float fog =
        fogAt({(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, 0.f});
    fillConvex(sink, q, 4, cityGray(0.3f, fog));
    fui::path(
        sink, {q[0].x, q[0].y, q[1].x, q[1].y}, 1.f, cityGray(0.9f, fog)
    );
    fui::path(
        sink, {q[2].x, q[2].y, q[3].x, q[3].y}, 1.f, cityGray(0.9f, fog)
    );
  };
  // Deck over the stretch of a straight road that lies on water.
  auto bridge = [&](V2 a, V2 b, float half) {
    const float len = std::hypot(b.x - a.x, b.y - a.y);
    float wet0 = -1.f, wet1 = -1.f;
    for (float d = 0.f; d <= len; d += 6.f) {
      const V2 p{
          a.x + (b.x - a.x) * d / len, a.y + (b.y - a.y) * d / len
      };
      if (distanceTo(river, p) < layout.riverHalf + 10.f) {
        if (wet0 < 0.f) wet0 = d;
        wet1 = d;
      }
    }
    if (wet0 < 0.f) return;
    wet0 = std::max(0.f, wet0 - 14.f),
    wet1 = std::min(len, wet1 + 14.f);
    deck(
        {a.x + (b.x - a.x) * wet0 / len, a.y + (b.y - a.y) * wet0 / len
        },
        {a.x + (b.x - a.x) * wet1 / len, a.y + (b.y - a.y) * wet1 / len
        },
        half
    );
  };
  const float loX = gridLine(layout, span.gx0),
              hiX = gridLine(layout, span.gx1);
  const float loY = gridLine(layout, span.gy0),
              hiY = gridLine(layout, span.gy1);
  for (int i = std::min(span.gx0, span.gy0);
       i <= std::max(span.gx1, span.gy1);
       ++i) {
    if (i % layout.avenueEvery != 0) continue;
    const float g = gridLine(layout, i),
                half = layout.avenue * 0.5f - 3.f;
    const bool alongY = i >= span.gx0 && i <= span.gx1,
               alongX = i >= span.gy0 && i <= span.gy1;
    if (alongY) bridge({g, loY}, {g, hiY}, layout.avenue * 0.5f);
    if (alongX) bridge({loX, g}, {hiX, g}, layout.avenue * 0.5f);
    constexpr int kSeg = 32;
    for (int k = 0; k < kSeg; ++k) {
      const float a = loY + (hiY - loY) * k / kSeg,
                  b = loY + (hiY - loY) * (k + 1) / kSeg;
      const float ax = loX + (hiX - loX) * k / kSeg,
                  bx = loX + (hiX - loX) * (k + 1) / kSeg;
      const float fog = fogAt({g, (a + b) * 0.5f, 0.f});
      const float fogX = fogAt({(ax + bx) * 0.5f, g, 0.f});
      for (float o : {-half, half}) {
        if (alongY)
          road(
              {g + o, a, 0},
              {g + o, b, 0},
              1.f,
              cityGray(0.95f, fog),
              0.f
          );
        if (alongX)
          road(
              {ax, g + o, 0},
              {bx, g + o, 0},
              1.f,
              cityGray(0.95f, fogX),
              0.f
          );
      }
      if (alongY)
        road({g, a, 0}, {g, b, 0}, 1.f, cityGray(0.6f, fog), 4.f);
      if (alongX)
        road({ax, g, 0}, {bx, g, 0}, 1.f, cityGray(0.6f, fogX), 4.f);
    }
  }
  // The route's own streets cross on bridges too.
  const std::vector<V3> route = routePath(layout);
  for (std::size_t i = 0; i + 1 < route.size(); ++i)
    bridge(
        {route[i].x, route[i].y},
        {route[i + 1].x, route[i + 1].y},
        layout.street * 0.5f + 2.f
    );

  // Buildings back to front (painter's order by footprint depth).
  std::vector<const Building*> order;
  order.reserve(buildings.size());
  for (const Building& b : buildings) order.push_back(&b);
  // Back to front by footprint; stacked tiers share a center, lowest
  // first.
  std::sort(
      order.begin(),
      order.end(),
      [&](const Building* a, const Building* b) {
        const float da = depth(
            cam, {(a->x0 + a->x1) * 0.5f, (a->y0 + a->y1) * 0.5f, 0.f}
        );
        const float db = depth(
            cam, {(b->x0 + b->x1) * 0.5f, (b->y0 + b->y1) * 0.5f, 0.f}
        );
        if (std::fabs(da - db) > 0.01f) return da > db;
        return a->z0 < b->z0;
      }
  );
  for (const Building* b : order) {
    const std::vector<Face> faces = visibleFaces(cam, *b);
    if (faces.empty()) continue;
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (const Face& f : faces)
      for (const V2& p : f.pts)
        minX = std::min(minX, p.x), maxX = std::max(maxX, p.x),
        minY = std::min(minY, p.y), maxY = std::max(maxY, p.y);
    if (maxX < box.x || minX > box.x + box.w || maxY < box.y ||
        minY > box.y + box.h)
      continue;
    const float fog = fogAt(
        {(b->x0 + b->x1) * 0.5f, (b->y0 + b->y1) * 0.5f, b->h * 0.5f}
    );
    for (std::size_t fi = 0; fi < faces.size(); ++fi) {
      const Face& f = faces[fi];
      fillConvex(sink, f.pts.data(), 4, cityGray(f.shade * 0.5f, fog));
      if (fi + 1 == faces.size()) break;
      // Ambient occlusion: the lower third of each wall darkens to the
      // street.
      if (b->z0 == 0.f) {
        const V2 q[4] = {
            f.pts[0],
            f.pts[1],
            {f.pts[1].x + (f.pts[2].x - f.pts[1].x) * 0.3f,
             f.pts[1].y + (f.pts[2].y - f.pts[1].y) * 0.3f},
            {f.pts[0].x + (f.pts[3].x - f.pts[0].x) * 0.3f,
             f.pts[0].y + (f.pts[3].y - f.pts[0].y) * 0.3f}
        };
        fillConvex(sink, q, 4, cityGray(f.shade * 0.3f, fog));
      }
      // Floor bands on lit tower walls (every ~4 floors); lost in the
      // haze.
      const float storey = 14.f, tall = b->h - b->z0;
      if (tall < 40.f || f.shade < 0.5f || fog > 0.6f) continue;
      for (float z = storey; z < tall - 4.f; z += storey) {
        const float t = z / tall;
        const V2 a{
            f.pts[0].x + (f.pts[3].x - f.pts[0].x) * t,
            f.pts[0].y + (f.pts[3].y - f.pts[0].y) * t
        };
        const V2 c{
            f.pts[1].x + (f.pts[2].x - f.pts[1].x) * t,
            f.pts[1].y + (f.pts[2].y - f.pts[1].y) * t
        };
        fui::path(
            sink,
            {a.x, a.y, c.x, c.y},
            1.f,
            cityGray(f.shade * 0.68f, fog)
        );
      }
    }
    // Roof rim: the crisp edge light that reads as a 3D model.
    const Face& roof = faces.back();
    fui::path(
        sink,
        {roof.pts[0].x,
         roof.pts[0].y,
         roof.pts[1].x,
         roof.pts[1].y,
         roof.pts[2].x,
         roof.pts[2].y,
         roof.pts[3].x,
         roof.pts[3].y,
         roof.pts[0].x,
         roof.pts[0].y},
        1.f,
        cityGray(0.85f, fog)
    );
    V2 foot, tip;
    const float mx = (b->x0 + b->x1) * 0.5f,
                my = (b->y0 + b->y1) * 0.5f;
    if (b->spire > 0.f && project(cam, {mx, my, b->h}, &foot) &&
        project(cam, {mx, my, b->h + b->spire}, &tip)) {
      fui::path(
          sink,
          {foot.x, foot.y, tip.x, tip.y},
          1.5f,
          cityGray(0.9f, fog)
      );
      sink.fillRounded(
          {tip.x - 1.5f, tip.y - 1.5f, 3.f, 3.f}, 1, cityGray(1.3f, fog)
      );
    }
  }
}

// Point `d` units along the route (looping) and its travel direction.
inline city::V3 routeAt(
    const std::vector<city::V3>& route, float d, city::V2* dir
) {
  float total = 0.f;
  for (std::size_t i = 0; i + 1 < route.size(); ++i)
    total += std::hypot(
        route[i + 1].x - route[i].x, route[i + 1].y - route[i].y
    );
  d = std::fmod(d, total);
  for (std::size_t i = 0; i + 1 < route.size(); ++i) {
    const float dx = route[i + 1].x - route[i].x,
                dy = route[i + 1].y - route[i].y;
    const float len = std::hypot(dx, dy);
    if (d <= len || i + 2 == route.size()) {
      *dir = {dx / len, dy / len};
      return {
          route[i].x + dx * d / len,
          route[i].y + dy * d / len,
          route[i].z
      };
    }
    d -= len;
  }
  return route.front();
}

// The flying car: a hot beacon hovering over the route with its shadow
// dart on the ground beneath.
inline void paintCraft(
    AnySink& sink,
    const city::Camera& cam,
    const std::vector<city::V3>& route,
    float travel
) {
  using namespace city;
  V2 dir;
  const V3 at = routeAt(route, travel + 380.f, &dir);
  paintGroundChevron(sink, cam, {at.x, at.y, 0.6f}, dir, 22.f);
  V2 g, p;
  if (!project(cam, {at.x, at.y, 0.6f}, &g) ||
      !project(cam, {at.x, at.y, 60.f}, &p))
    return;
  const std::vector<float> tether{g.x, g.y, p.x, p.y};
  sink.polyline(tether.data(), 2, 1.f, 3.f, 0xfa780890u);
  sink.pushBlend(BlendMode::Plus);
  sink.fillRounded(
      {p.x - 16.f, p.y - 16.f, 32.f, 32.f}, 16, 0xfa780828u
  );
  sink.fillRounded({p.x - 8.f, p.y - 8.f, 16.f, 16.f}, 8, 0xfa780860u);
  sink.pop();
  sink.fillRounded({p.x - 3.5f, p.y - 3.5f, 7.f, 7.f}, 3, 0xffc070ffu);
}

// Live layer: the route HUD (glow, dotted core, ground darts).
inline void paintCityRoute(
    AnySink& sink, const city::Camera& cam, float travel
) {
  using namespace city;
  static const Layout layout;
  const std::vector<V3> route = routePath(layout);
  std::vector<float> pts;
  for (std::size_t i = 0; i + 1 < route.size(); ++i) {
    for (int s = 0; s <= 24; ++s) {
      const float t = static_cast<float>(s) / 24.f;
      V2 p;
      if (project(
              cam,
              {route[i].x + (route[i + 1].x - route[i].x) * t,
               route[i].y + (route[i + 1].y - route[i].y) * t,
               route[i].z},
              &p
          )) {
        pts.push_back(p.x);
        pts.push_back(p.y);
      }
    }
  }
  sink.pushBlend(BlendMode::Plus);
  sink.polyline(pts.data(), pts.size() / 2, 10.f, 0.f, 0xfa780820u);
  sink.polyline(pts.data(), pts.size() / 2, 4.f, 0.f, 0xfa780848u);
  sink.pop();
  sink.polyline(
      pts.data(), pts.size() / 2, 2.f, 5.f, fui::Ref::kOrange
  );
  // Darts advance with the craft: their phase is the distance flown.
  constexpr float kPitch = 150.f;
  float carry = 60.f + std::fmod(travel, kPitch);
  for (std::size_t i = 0; i + 1 < route.size(); ++i) {
    const float dx = route[i + 1].x - route[i].x,
                dy = route[i + 1].y - route[i].y;
    const float len = std::hypot(dx, dy);
    for (float d = carry; d < len; d += kPitch) {
      paintGroundChevron(
          sink,
          cam,
          {route[i].x + dx * d / len, route[i].y + dy * d / len, 0.6f},
          {dx / len, dy / len},
          13.f
      );
      carry = d + kPitch - len;
    }
  }
  paintCraft(sink, cam, route, travel);
}

// Unbaked fallback: the whole city as vectors.
inline void paintCityMap(
    const Box& box, AnySink& sink, const Engine& e, float travel
) {
  const CityView v = cityView(box);
  const city::Camera cam = cityCamera(v);
  sink.fill(box, 0x08090bff);
  paintCityGround(box, sink, cam);
  paintCityRoute(sink, cam, travel);
  paintSubtleMapVignette(box, sink, e);
}

}  // namespace toolkit
}  // namespace glim
