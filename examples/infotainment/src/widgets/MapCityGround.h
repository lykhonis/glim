#pragma once

// Glim paint has no polygon shape, so convex faces are filled with a
// one-pixel horizontal serpentine polyline: polyline coverage is the
// union of its segments, so rows at pixel pitch with unit width are a
// solid, anti-aliased fill on every backend (CPU and GPU share the
// strip path).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Fui.h"
#include "MapCityModel.h"
#include "MapMesh.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

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

}  // namespace toolkit
}  // namespace glim
