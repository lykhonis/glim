#pragma once

// Paint interpretation for toolkit trees: feed provisioning, the pixel
// sink (one method per primitive — no widget switch), atlas measuring,
// one-screen paint.
#include <glim/paint/Context.h>

#include <cmath>
#include <cstdint>
#include <typeinfo>

#include "glim/toolkit/Emit.h"
#include "glim/toolkit/Measure.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Theme.h"
#include "glim/toolkit/Tokens.h"
#include "glim/toolkit/Toolkit.h"

namespace glim {
namespace toolkit {
namespace host {

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
  // Legacy peers without paint ride entries; the toolkit no longer
  // emits any, so this ignores unknown kinds.
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
    // y is the TOP of the line box; Context::text takes a BASELINE
    // origin. Without this every label sits one ascent too high.
    // Snapped to half-logical (one device pixel at 2x) so glyph
    // edges never straddle rows; layout boxes stay untouched.
    auto snap = [](float v) { return std::floor(v * 2.0f + 0.5f) / 2.0f; };
    ::glim::paint::TextSize m = ctx.measureText(s.c_str(), size);
    ctx.setFillColor(rgba);
    ctx.text({snap(x), snap(y + m.ascent)}, s.c_str(), size);
  }
  void image(const Box& b, std::uint32_t imageId, bool foreign, const std::string& = {}) {
    if (imageId == 0) return;
    ctx.blit(
        toRect(b),
        foreign ? ::glim::paint::Matter::foreign(imageId) : ::glim::paint::Matter::sampled(imageId)
    );
  }
  void pushGlass(const Box& b, int radius, bool clear) {
    ::glim::paint::GroupParams p;
    p.bounds = toRect(b);
    p.clip = toRect(b);
    p.clipRadius = Radius{static_cast<float>(radius)};
    ::glim::paint::Glass g = clear ? ::glim::paint::Glass::clear() : ::glim::paint::Glass{};
    g.thicknessPx = theme(Engine{}).glassThickness;
    p.glass = g;
    ctx.pushGroup(p);
  }
  void pushClip(const Box& b) {
    ::glim::paint::GroupParams p;
    p.clip = toRect(b);
    ctx.pushGroup(p);
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
  static Env node{&meas, &typeid(Measurer), nullptr};
  return Engine{&node};
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
  ctx.setFillColor(theme(engine).windowBg);
  ctx.fill(glim::Rect{{0, 0}, ctx.size()});
  ContextSink sink{ctx};
  paintTree(tree, box, engine, sink, feed);
  paintFocusRing(tree, box, focus, sink, engine, focusVisible);
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
