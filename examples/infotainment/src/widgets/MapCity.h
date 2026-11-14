#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Fui.h"
#include "MapCityGround.h"
#include "MapCityModel.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

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
        route[i].x + dx * d / len, route[i].y + dy * d / len, route[i].z
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
