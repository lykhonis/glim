#pragma once

// Paint interpretation for toolkit trees: feed provisioning, pixel sink,
// atlas measuring, one-screen paint.
#include <glim/paint/Context.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <typeinfo>

#include "glim/toolkit/Emit.h"
#include "glim/toolkit/Measure.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tokens.h"
#include "glim/toolkit/Toolkit.h"
#include "glim/toolkit/elements/Roll.h"
#include "glim/toolkit/theme/Keys.h"

namespace glim {
namespace toolkit {
namespace host {

namespace detail {
// Packed RGBA with alpha scaled by a clamped to 0..1, rounded.
inline std::uint32_t scaleAlpha(std::uint32_t rgba, float a) {
  a = a < 0.f ? 0.f : (a > 1.f ? 1.f : a);
  return (rgba & 0xffffff00u) | (static_cast<unsigned>((rgba & 0xffu) * a + 0.5f) & 0xffu);
}
}  // namespace detail

struct LayerFeed {
  std::uint32_t id = 0;
  bool foreign = false;

  std::uint32_t ensure(::glim::paint::Context& ctx) {
    if (id != 0) return id;
    constexpr int k = 64;
    static std::uint8_t px[k * k * 4];
    static bool baked = false;
    if (!baked) {
      for (int y = 0; y < k; ++y)
        for (int x = 0; x < k; ++x) {
          bool cell = ((x / 8) + (y / 8)) % 2 == 0;
          std::size_t i = static_cast<std::size_t>((y * k + x) * 4);
          px[i + 0] = cell ? 0x2a : 0xf0;
          px[i + 1] = cell ? 0x9d : 0xd5;
          px[i + 2] = cell ? 0x8f : 0xa8;
          px[i + 3] = 0xff;
        }
      baked = true;
    }
    id = ctx.addImage(k, k, px);
    foreign = false;
    return id;
  }
};

inline Rect toRect(const Box& b) { return Rect{{b.x, b.y}, {b.w, b.h}}; }

struct ContextSink {
  ::glim::paint::Context& ctx;
  explicit ContextSink(::glim::paint::Context& c) : ctx(c) {}
  ::glim::paint::Context& context() { return ctx; }
  // Legacy peers without paint ride entries; the toolkit no longer emits any.
  template <class F>
  void entry(const Rec::Entry&, F&) {}
  void fill(const Box& b, std::uint32_t rgba, const std::string& = {}) {
    ctx.setFillColor(rgba);
    ctx.fill(toRect(b));
  }
  void fillRounded(const Box& b, int radius, std::uint32_t rgba, const std::string& = {}) {
    ctx.setFillColor(rgba);
    ctx.fillRounded(toRect(b), Radius{static_cast<float>(radius)});
  }
  void fillGradient(
      const Box& b, int radius, std::uint32_t top, std::uint32_t bottom, const std::string& = {}
  ) {
    ::glim::paint::GradientStop stops[2] = {
        {0.0f, ::glim::Color{top}},
        {1.0f, ::glim::Color{bottom}},
    };
    ctx.setFill(::glim::paint::Matter::linearGradient({b.x, b.y}, {b.x, b.y + b.h}, stops, 2));
    ctx.fillRounded(toRect(b), Radius{static_cast<float>(radius)});
  }
  void text(
      float x,
      float y,
      const std::string& s,
      std::uint32_t rgba,
      float size,
      const std::string& = {}
  ) {
    // y is the TOP of the line box; snapped to half-logical (one device
    // pixel at 2x) so glyph edges never straddle rows.
    ::glim::paint::TextSize m = ctx.measureText(s.c_str(), size);
    ctx.setFillColor(rgba);
    ctx.text(
        {toolkit::detail::snapHalf(x), toolkit::detail::snapHalf(y + m.ascent)}, s.c_str(), size
    );
  }
  void textStyled(
      float x,
      float y,
      const std::string& s,
      std::uint32_t rgba,
      float size,
      float condense,
      float weight,
      const std::string& = {}
  ) {
    ::glim::paint::TextSize m = ctx.measureTextStyled(s.c_str(), size, condense);
    ctx.setFillColor(rgba);
    ctx.textStyled(
        {toolkit::detail::snapHalf(x), toolkit::detail::snapHalf(y + m.ascent)},
        s.c_str(),
        size,
        condense,
        weight
    );
  }
  void image(const Box& b, std::uint32_t imageId, bool foreign, const std::string& = {}) {
    if (imageId == 0) return;
    ctx.blit(
        toRect(b),
        foreign ? ::glim::paint::Matter::foreign(imageId) : ::glim::paint::Matter::sampled(imageId)
    );
  }
  void pushGlass(
      const Box& b,
      int radius,
      bool clear,
      std::uint8_t edge,
      std::uint32_t tint,
      std::uint8_t material = 0
  ) {
    ::glim::paint::GroupParams p;
    p.bounds = toRect(b);
    p.clip = toRect(b);
    p.clipRadius = Radius{static_cast<float>(radius)};
    // Glass thickness is a physical constant, never a scoped token, so read
    // the default directly instead of plumbing an engine through the sink.
    // Twin enums; the asserts pin the shared bit layout so drift fails the build.
    using PaintEdge = ::glim::paint::LensEdge;
    static_assert(static_cast<int>(toolkit::GlassEdge::None) == static_cast<int>(PaintEdge::None));
    static_assert(static_cast<int>(toolkit::GlassEdge::Left) == static_cast<int>(PaintEdge::Left));
    static_assert(
        static_cast<int>(toolkit::GlassEdge::Right) == static_cast<int>(PaintEdge::Right)
    );
    static_assert(static_cast<int>(toolkit::GlassEdge::Top) == static_cast<int>(PaintEdge::Top));
    static_assert(
        static_cast<int>(toolkit::GlassEdge::Bottom) == static_cast<int>(PaintEdge::Bottom)
    );
    static_assert(static_cast<int>(toolkit::GlassEdge::Full) == static_cast<int>(PaintEdge::Full));
    using PaintMaterial = ::glim::paint::LensMaterial;
    static_assert(
        static_cast<int>(toolkit::GlassMaterial::Glass) == static_cast<int>(PaintMaterial::Glass)
    );
    static_assert(
        static_cast<int>(toolkit::GlassMaterial::LiquidMetal) ==
        static_cast<int>(PaintMaterial::LiquidMetal)
    );
    static_assert(
        static_cast<int>(toolkit::GlassMaterial::Water) == static_cast<int>(PaintMaterial::Water)
    );
    ::glim::paint::Lens g;
    if (material == static_cast<std::uint8_t>(toolkit::GlassMaterial::LiquidMetal)) {
      g = ::glim::paint::Lens::liquidMetal(::glim::Color{tint});
    } else if (material == static_cast<std::uint8_t>(toolkit::GlassMaterial::Water)) {
      g = ::glim::paint::Lens::water();
      if (tint != 0) {
        g.tint = ::glim::Color{tint};
      }
    } else {
      g = clear ? ::glim::paint::Lens::clear() : ::glim::paint::Lens{};
    }
    g.thicknessPx = Tokens::kGlassThickness;
    g.edge = static_cast<PaintEdge>(edge);
    // Flat plates are panels, not droplets: they never melt into neighbors
    // and have no lens bevel, so their sides stay square.
    if (edge == static_cast<std::uint8_t>(toolkit::GlassEdge::None)) {
      g.mergeKPx = 0.f;
      g.thicknessPx = 0.f;
    }
    g.tint = ::glim::Color{tint};
    p.lens = g;
    ctx.pushGroup(p);
  }
  void pushClip(const Box& b) {
    ::glim::paint::GroupParams p;
    p.clip = toRect(b);
    ctx.pushGroup(p);
  }
  // Additive scope for speculars: Plus adds light instead of washing
  // toward gray like SrcOver white does. Merge keeps it a pipeline switch.
  void pushBlend(BlendMode m) {
    ::glim::paint::GroupParams p;
    p.blend = m == BlendMode::Plus ? ::glim::paint::Blend::Plus : ::glim::paint::Blend::SrcOver;
    ctx.pushGroup(p);
  }
  // Merge folds a lone shape's fade into its tint; anything more
  // composites once through an isolate.
  void pushOpacity(float opacity) {
    ::glim::paint::GroupParams p;
    p.opacity = std::clamp(opacity, 0.f, 1.f);
    ctx.pushGroup(p);
  }
  void pushTransform(float dx, float dy) {
    ::glim::paint::GroupParams p;
    p.transform = ::glim::Mat4::translate(dx, dy);
    ctx.pushGroup(p);
  }
  void pushRotate(float angleDeg, float cx, float cy) {
    const float rad = angleDeg * 3.14159265f / 180.0f;
    ::glim::Mat4 m = ::glim::Mat4::translate(cx, cy) * ::glim::Mat4::rotateZ(rad) *
                     ::glim::Mat4::translate(-cx, -cy);
    ::glim::paint::GroupParams p;
    p.transform = m;
    ctx.pushGroup(p);
  }
  void fillRadial(
      const Box& b, int radius, std::uint32_t center, std::uint32_t edge, const std::string& = {}
  ) {
    const float cx = b.x + b.w * 0.5f, cy = b.y + b.h * 0.5f;
    ::glim::paint::GradientStop stops[2] = {
        {0.0f, ::glim::Color{center}},
        {1.0f, ::glim::Color{edge}},
    };
    ctx.setFill(
        ::glim::paint::Matter::radialGradient({cx, cy}, 0.5f * std::hypot(b.w, b.h), stops, 2)
    );
    ctx.fillRounded(toRect(b), Radius{static_cast<float>(radius)});
  }
  void fillChamfer(const Box& b, float cut, std::uint32_t rgba, const std::string& = {}) {
    ctx.setFillColor(rgba);
    ctx.fillChamfer(toRect(b), cut);
  }
  void strokeChamfer(
      const Box& b, float cut, float width, std::uint32_t rgba, const std::string& = {}
  ) {
    ctx.setFillColor(rgba);
    ctx.strokeChamfer(toRect(b), cut, width);
  }
  void arc(
      float cx,
      float cy,
      float radius,
      float thickness,
      float startDeg,
      float sweepDeg,
      std::uint32_t rgba,
      const std::string& = {}
  ) {
    ctx.setFillColor(rgba);
    ::glim::paint::Arc a;
    a.center = {cx, cy};
    a.radius = radius;
    a.thickness = thickness;
    a.startDeg = startDeg;
    a.sweepDeg = sweepDeg;
    ctx.arc(a);
  }
  void polyline(
      const float* xy,
      std::size_t n,
      float width,
      float dotPitch,
      std::uint32_t rgba,
      const std::string& = {}
  ) {
    ctx.setFillColor(rgba);
    ::glim::paint::Polyline pl;
    pl.width = width;
    pl.dotPitch = dotPitch;
    pl.points.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      pl.points.push_back({xy[i * 2], xy[i * 2 + 1]});
    }
    ctx.polyline(pl);
  }
  // Opacity fades the caster only. The shadow blit stays full strength, so
  // a plate can cast without covering the backdrop the glass samples.
  void pushShadow(
      float sigma, float ox, float oy, std::uint32_t color, float expand, float opacity = 1.f
  ) {
    ::glim::paint::GroupParams p;
    ::glim::paint::ShadowBlur sb;
    sb.sigma = sigma;
    sb.offset = {ox, oy};
    sb.color = ::glim::Color{color};
    sb.expandPx = expand;
    p.shadow = sb;
    p.opacity = opacity;
    ctx.pushGroup(p);
  }
  // Effect scope. Glow lowers to an additive radial gradient: color at
  // `intensity` alpha in the center, transparent at the edge. No isolate,
  // no blur.
  void pushEffect(
      const Box& b, int radius, std::uint32_t kind, std::uint32_t color, float intensity
  ) {
    ::glim::paint::GroupParams p;
    p.blend = ::glim::paint::Blend::Plus;
    ctx.pushGroup(p);
    if (kind == static_cast<std::uint32_t>(toolkit::EffectKind::Glow))
      fillRadial(b, radius, detail::scaleAlpha(color, intensity), color & 0xffffff00u);
  }
  void pop() { ctx.popGroup(); }
  void slot(const Box& b, std::uint32_t id) { ctx.slot(toRect(b), id); }
  void stroke(const Box& b, int radius, float width, std::uint32_t rgba, const std::string& = {}) {
    ctx.setFillColor(rgba);
    ctx.strokeRect(toRect(b), Radius{static_cast<float>(radius)}, width);
  }
};

inline float measureBridge(const char* text, float sizePx) {
  static glim::paint::Context measurer;
  return measurer.measureText(text, sizePx).width;
}

inline Engine measureEngine() {
  static Measurer meas{&measureBridge};
  static Env node{&meas, &typeid(Measurer), std::nullopt};
  return Engine{node, StoreRef{}};
}

inline void paintScrollbars(
    glim::paint::Context& ctx, const Block& tree, const Box& box, const Engine& engine
) {
  // Thin overlay thumbs for scrollable rolls, drawn last. Shares
  // rollMetrics with Screen::wheel so thumb-feel and hit-feel agree.
  ContextSink sink{ctx};
  walk(tree, box, engine, [&](const Block& n, const Box& bx) {
    const Roll* r = rollOf(n);
    if (!r || !r->scrollOff) return;
    RollMetrics m = rollMetrics(*r, bx, engine);
    if (m.maxOff <= 0.0f || m.viewport <= 0.0f) return;
    if (m.horizontal) {
      float w = m.viewport * m.viewport / m.content;
      w = w < 24.0f ? 24.0f : (w > m.viewport ? m.viewport : w);
      const float x = bx.x + (m.offset / m.maxOff) * (m.viewport - w);
      sink.fillRounded(
          {toolkit::detail::snapHalf(x), toolkit::detail::snapHalf(bx.y + bx.h - 6.0f), w, 4.0f},
          2,
          0xffffff40
      );
    } else {
      float h = m.viewport * m.viewport / m.content;
      h = h < 24.0f ? 24.0f : (h > m.viewport ? m.viewport : h);
      const float y = bx.y + (m.offset / m.maxOff) * (m.viewport - h);
      sink.fillRounded(
          {toolkit::detail::snapHalf(bx.x + bx.w - 6.0f), toolkit::detail::snapHalf(y), 4.0f, h},
          2,
          0xffffff40
      );
    }
  });
}

inline void paintScreen(
    glim::paint::Context& ctx,
    const Block& tree,
    const Box& box,
    const Engine& engine,
    LayerFeed& feed,
    int focus = -1,
    bool focusVisible = true
) {
  feed.ensure(ctx);
  // The clear color lives under the tree's own theme scope: a root
  // themed() must tint the backdrop, not just its children. Resolve
  // with the root scope applied; non-themed roots keep the exact
  // previous behavior (same engine, same fallback).
  Engine bgEngine = engine;
  ThemeScope bgScope;
  Env bgNode;
  // Custom-wrapped root (themed() stores inline as Custom): borrow
  // its scope for the backdrop fill. Plain roots skip this and keep
  // the exact previous behavior.
  if (auto c = tree.get<Custom>()) {
    if (auto th = c->get().as<Themed>()) {
      bgScope = th->get().themeScope();
      bgNode = Env{&bgScope, &typeid(ThemeScope), engine.env};
      bgEngine.env = bgNode;
    }
  }
  ctx.setFillColor(themeColor(bgEngine, kThemeWindowBg));
  ctx.fill(glim::Rect{{0, 0}, ctx.size()});
  ContextSink sink{ctx};
  paintTree(tree, box, engine, sink, feed);
  paintFocusRing(tree, box, focus, sink, engine, focusVisible);
  paintScrollbars(ctx, tree, box, engine);
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
