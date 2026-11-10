#pragma once

// MapPerspective: CPU-projection 3D map backdrop — the cross-platform
// half of the dual 3D strategy (see Map3D.h, docs/compositor.md).
//
// Why CPU projection: Glim v1 GPU is vertex+fragment only (K29) with no
// required compute/mesh path, and the same Scene must run on
// Metal/Vulkan/WebGPU/CPU. Projecting the ground mesh + arterials +
// route on the CPU and emitting the *existing* 2D ops (polyline, fill,
// Plus blend, gradients) gives perspective-foreshortened streets and a
// converging route with zero new paint ops, zero new shaders, and
// identical pixels on every backend including software raster.
//
// What it is: a pinhole-over-ground-plane projection. World X in
// [-1,1], depth d in [0,1] (0 = bumper/bottom, 1 = horizon). Screen:
//   persp = 1/(1+d*K), sx = cx + xn*halfW*persp,
//   sy = horizonY + (bottomY-horizonY)*(1-d)^gamma*persp-ish.
// Longitudinal world lines converge to the vanishing point,
// transverse lines compress toward the horizon — the reference
// paQys.jpg read (streets + dotted route diving under the cards).
//
// Foreign texture (Map3D ensureMap3DFeed) stays the live-3D half:
// a real 3D engine's texture blitted in-scene under glass/Plus.
// This file is the procedural half: no native handles, deterministic,
// golden-testable.

#include <cmath>
#include <cstdint>

#include "../InfoTheme.h"
#include "MapMesh.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct MapCamera {
  // Perspective strength (higher = faster convergence).
  float k = 3.5f;
  // Ground foreshortening exponent.
  float gamma = 1.35f;
  // Horizon line as a fraction of box height. Negative pushes the
  // vanishing point above the frame so streets reach the top edge.
  float horizonFrac = -0.30f;
};

inline float mapPersp(const MapCamera& cam, float d) {
  return 1.f / (1.f + d * cam.k);
}

inline void mapProject(
    const Box& box,
    const MapCamera& cam,
    float xn,
    float d,
    float* sx,
    float* sy
) {
  const float cx = box.x + box.w * 0.5f;
  const float horizonY = box.y + box.h * cam.horizonFrac;
  const float bottomY = box.y + box.h;
  const float p = mapPersp(cam, d);
  *sx = cx + xn * (box.w * 0.5f) * p;
  const float t = 1.f - d;
  const float g =
      std::pow(t < 0.f ? 0.f : (t > 1.f ? 1.f : t), cam.gamma);
  *sy = horizonY + (bottomY - horizonY) * g * (0.35f + 0.65f * p);
}

inline void paintProjectedMap(
    const Box& box,
    AnySink& sink,
    const Engine& e,
    const MapCamera& cam = MapCamera{}
) {
  const std::uint32_t bgTop =
      themeColor(e, kInfoCardTop, Info::kCardTop);
  const std::uint32_t bgBottom =
      themeColor(e, kInfoCardBottom, Info::kCardBottom);

  sink.fillGradient(box, 8, bgTop, bgBottom);

  const std::uint32_t street = 0x8fa0b5ff;
  const std::uint32_t waypoint = 0xe8eef5ff;
  // Longitudinal streets: constant world X, sampled in depth so the
  // polyline itself carries the perspective curve. 15 lines.
  for (int li = 0; li <= 14; ++li) {
    const float xn = -1.f + 2.f * static_cast<float>(li) / 14.f;
    float pts[32];
    const int n = 16;
    for (int i = 0; i < n; ++i) {
      const float d = static_cast<float>(i) / static_cast<float>(n - 1);
      float sx, sy;
      mapProject(box, cam, xn, d, &sx, &sy);
      pts[i * 2] = sx;
      pts[i * 2 + 1] = sy;
    }
    const float edge = (li == 0 || li == 10) ? 0.5f : 1.f;
    sink.polyline(pts, n, 1.5f * edge, 0.f, withAlpha(street, 0.30f));
  }
  // Transverse streets: constant depth, compressed toward horizon.
  for (int ti = 0; ti < 14; ++ti) {
    const float d = static_cast<float>(ti) / 13.f;
    float pts[32];
    const int n = 16;
    for (int i = 0; i < n; ++i) {
      const float xn = -1.f + 2.f * static_cast<float>(i) /
                                  static_cast<float>(n - 1);
      float sx, sy;
      mapProject(box, cam, xn, d, &sx, &sy);
      pts[i * 2] = sx;
      pts[i * 2 + 1] = sy;
    }
    sink.polyline(pts, n, 1.f, 0.f, withAlpha(street, 0.26f));
  }
  // Cracked shorts in world space: hashed (x0,z0,len) projected.
  for (int i = 0; i < 14; ++i) {
    const float x0 = mapHash01(i * 2 + 501) * 2.f - 1.f;
    const float z0 = mapHash01(i * 2 + 507);
    const float len = 0.04f + mapHash01(i * 3 + 513) * 0.10f;
    const bool horiz = (i % 3) != 2;
    float pts[4];
    if (horiz) {
      float ax, ay, bx, by;
      mapProject(box, cam, x0, z0, &ax, &ay);
      mapProject(box, cam, x0 + len, z0, &bx, &by);
      pts[0] = ax;
      pts[1] = ay;
      pts[2] = bx;
      pts[3] = by;
    } else {
      float ax, ay, bx, by;
      mapProject(box, cam, x0, z0, &ax, &ay);
      mapProject(box, cam, x0, z0 + len * 0.5f, &bx, &by);
      pts[0] = ax;
      pts[1] = ay;
      pts[2] = bx;
      pts[3] = by;
    }
    sink.polyline(pts, 2, 1.f, 0.f, withAlpha(street, 0.30f));
  }
  // Route: world-space zigzag dives to the horizon, Plus dotted orange.
  {
    const float wx[8] = {
        0.30f, 0.38f, 0.32f, 0.40f, 0.34f, 0.42f, 0.30f, 0.22f
    };
    const float wz[8] = {
        0.02f, 0.14f, 0.26f, 0.38f, 0.50f, 0.62f, 0.76f, 0.90f
    };
    float route[16];
    float carX = 0.f, carY = 0.f;
    for (int i = 0; i < 8; ++i) {
      float sx, sy;
      mapProject(box, cam, wx[i], wz[i], &sx, &sy);
      route[i * 2] = sx;
      route[i * 2 + 1] = sy;
      if (i == 0) {
        carX = sx;
        carY = sy;
      }
    }
    const std::uint32_t accent =
        themeColor(e, kInfoAccent, Info::kAccent);
    sink.pushBlend(BlendMode::Plus);
    sink.polyline(route, 8, 2.5f, 6.f, accent);
    sink.pop();
    // Ego marker at the route's near end (screen-space chevron).
    // No halo quad: a radial glow rasterizes as a solid block at this
    // size.
    float chev[10] = {
        carX,
        carY - 8.f,
        carX + 7.f,
        carY + 6.f,
        carX,
        carY + 2.f,
        carX - 7.f,
        carY + 6.f,
        carX,
        carY - 8.f
    };
    sink.polyline(chev, 5, 2.f, 0.f, 0xffffffffu);
    sink.fillRounded({carX - 2.f, carY - 2.f, 4.f, 4.f}, 2, accent);
  }
  // Waypoint triangles scattered on the mesh (reference paQys markers).
  {
    const float wpx[3] = {-0.45f, 0.05f, 0.62f};
    const float wpz[3] = {0.45f, 0.62f, 0.30f};
    for (int i = 0; i < 3; ++i) {
      float sx, sy;
      mapProject(box, cam, wpx[i], wpz[i], &sx, &sy);
      float tri[8] = {
          sx - 5.f, sy, sx + 5.f, sy, sx, sy - 7.f, sx - 5.f, sy
      };
      sink.polyline(tri, 4, 1.5f, 0.f, withAlpha(waypoint, 0.35f));
    }
  }
  paintSubtleMapVignette(box, sink, e);
}

struct MapPerspective {
  static constexpr const char* kKind = "MapPerspective";
  float width = 1168.f;
  float height = 784.f;
  MapCamera camera;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(width, height, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 8);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    paintProjectedMap(box, sink, e, camera);
  }
};

template <class... As>
Block mapPerspective(MapPerspective m, As&&... as) {
  configureTags(m, std::forward<As>(as)...);
  return finish(std::move(m), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
