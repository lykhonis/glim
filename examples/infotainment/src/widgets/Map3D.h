#pragma once

// Map3D: GPU 3D-map layer for the cluster backdrop (paQys.jpg: dark 3D
// streets with the dotted route, seen through/around glass panels).
//
// Two paths, one widget:
// - Foreign (Metal session): the app renders its 3D scene into its own
//   texture each frame; ensureMap3DFeed() wraps the native handle and
//   the widget blits it with Matter::foreign. It lands IN the Glim
//   scene, so glass blur refraction, Plus route glow, vignette and
//   shadows all composite over/around it like any other quad. Foreign
//   content is volatile by design: hash carries the native handle and
//   packet reuse is skipped (Reuse.cpp hasForeign), tile-diffing still
//   bounds upload.
// - Fallback (headless/golden/no provider): the exact procedural subtle
//   mesh from MapMesh.h, deterministic.
//
// Lifetime: ensureMap3DFeed() runs in the record lambda (has Context).
// It releases the previous wrap before re-wrapping, and keeps the id
// when the provider hands back the same handle (pool-friendly: no slot
// churn). Slot release only clears in place; ids are never reused
// mid-flight because the scene holds the store's ImageStore copy.
// Route line stays a Glim polyline on top (AA + theme + zero re-tess
// cost); a live provider shares its camera projection by handing
// screen-space points to a future routePts param (hook documented,
// default zigzag kept until then).

#include <cstdint>

#include "../InfoTheme.h"
#include "MapCity.h"
#include "MapMesh.h"
#include "MapPerspective.h"
#include "glim/paint/Context.h"
#include "glim/paint/Software.h"
#include "glim/toolkit.h"
#include "glim/toolkit/host/Paint.h"

namespace glim {
namespace toolkit {

struct Map3DProvider {
  virtual ~Map3DProvider() = default;
  // Return the current frame's native texture handle (e.g. MTLTexture*,
  // CVPixelBufferRef) with dimensions. Return nullptr when no 3D frame
  // is available (headless, fallback path). Handle must stay alive
  // through present.
  virtual void* nativeTexture(int* w, int* h) = 0;
};

struct Map3DFeed {
  std::uint32_t imageId = 0;
  bool foreign = false;
  void* handle = nullptr;
  // Baked city plate (ensureCityPlate); 0 paints the city as vectors.
  std::uint32_t cityId = 0;
  const void* cityOwner = nullptr;  // ids are per Context
};

// Rasterizes the static city once per process (CPU, deterministic) and
// registers it with this context: per-frame record then blits one image
// instead of thousands of face strips. The route stays live on top.
inline void ensureCityPlate(glim::paint::Context& ctx, Store& store) {
  State<Map3DFeed> st = store.state("map3d", Map3DFeed{});
  if (st.get().cityId != 0 && st.get().cityOwner == &ctx) {
    auto img = ctx.images().get(st.get().cityId);
    if (img && img->get().width == static_cast<int>(kCityPlateW))
      return;
  }
  static const std::vector<std::uint8_t> rgba = [] {
    const int w = static_cast<int>(kCityPlateW),
              h = static_cast<int>(kCityPlateH);
    const Box plate{0.f, 0.f, kCityPlateW, kCityPlateH};
    glim::paint::Context bake;
    bake.setSize({kCityPlateW, kCityPlateH});
    bake.beginFrame();
    host::ContextSink cs(bake);
    host::LayerFeed lf;
    AnySink sink = wrapSink(cs, lf);
    paintCityGround(plate, sink, cityCamera(cityView(plate)));
    bake.finish();
    std::vector<std::uint8_t> px(static_cast<std::size_t>(w) * h * 4);
    glim::paint::rasterScene(bake.scene(), w, h, px.data());
    return px;
  }();
  Map3DFeed f = st.get();
  f.cityId = ctx.addImage(
    static_cast<int>(kCityPlateW),
    static_cast<int>(kCityPlateH),
    rgba.data()
  );
  f.cityOwner = &ctx;
  st.set(f);
}

inline void ensureMap3DFeed(
  glim::paint::Context& ctx, Map3DFeed& feed, Map3DProvider* provider
) {
  int w = 0, h = 0;
  void* native = provider ? provider->nativeTexture(&w, &h) : nullptr;
  if (!native) {
    if (feed.imageId != 0) {
      ctx.releaseImage(feed.imageId);
      feed = Map3DFeed{};
    }
    return;
  }
  if (native == feed.handle && feed.imageId != 0) {
    return;
  }
  if (feed.imageId != 0) {
    ctx.releaseImage(feed.imageId);
  }
  feed.imageId = ctx.wrapNativeTexture(native, w, h);
  feed.foreign = feed.imageId != 0;
  feed.handle = feed.foreign ? native : nullptr;
  if (!feed.foreign) {
    feed.imageId = 0;
  }
}

struct Map3D {
  static constexpr const char* kKind = "Map3D";
  float width = 1168.f;
  float height = 784.f;
  // Opt-in CPU-projection fallback (cross-platform 3D look, no native
  // texture). Default false keeps the cluster golden bit-identical to
  // the flat procedural mesh; a live session enables it with
  // map3d({.perspective = true}) or wires a real provider instead.
  bool perspective = true;
  // Extruded 3D city with the glowing route; wins over perspective.
  bool city = false;
  // Route distance flown (Flight::travel): darts flow, the craft
  // advances.
  float travel = 0.f;
  MapCamera camera;
  // Backdrop: fills the window, design size only when unbounded.
  Box place(Constraints c, const Engine&) const {
    return {
      0.f,
      0.f,
      isUnbounded(c.maxW) ? width : c.maxW,
      isUnbounded(c.maxH) ? height : c.maxH
    };
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 8);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    Map3DFeed feed;
    if (auto st = e.store) {
      feed = *st->get().state("map3d", Map3DFeed{});
    }
    if (feed.foreign && feed.imageId != 0) {
      sink.image(box, feed.imageId, true);
      // Route + vignette stay Glim-side (crisp AA, theme accent).
      // Provider route projection hook: screen-space points land here
      // via a future routePts param; default zigzag until then.
      paintSubtleMapRoute(box, sink, e);
      paintSubtleMapVignette(box, sink, e);
      return;
    }
    if (city && feed.cityId != 0) {
      const CityView v = cityView(box);
      sink.fill(box, 0x08090bff);
      sink.image(v.plate, feed.cityId, false);
      paintCityRoute(sink, cityCamera(v), travel);
      paintSubtleMapVignette(box, sink, e);
      return;
    }
    if (city) {
      paintCityMap(box, sink, e, travel);
      return;
    }
    if (perspective) {
      paintProjectedMap(box, sink, e, camera);
      return;
    }
    paintSubtleMap(box, sink, e);
  }
};

template <class... As>
Block map3d(Map3D m, As&&... as) {
  configureTags(m, std::forward<As>(as)...);
  return finish(std::move(m), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
