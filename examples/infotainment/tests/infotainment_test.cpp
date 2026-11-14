#include "Infotainment.h"

#include "RecordBudget.h"

#include <glim/image/Image.h>
#include <glim/image/Png.h>
#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>
#include <glim/paint/Scene.h>
#include <glim/paint/Software.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "glim/toolkit.h"
#include "glim/toolkit/host/Codec.h"
#include "glim/toolkit/host/Host.h"
#include "glim/toolkit/host/Paint.h"
#include "widgets/MapPerspective.h"

using namespace glim::toolkit;

std::string infotainmentDump(Store& store);
std::string infotainmentA11yDump(Store& store);
void paintInfotainmentScene(
  glim::paint::Context& ctx,
  glim::toolkit::host::LayerFeed& feed,
  Store& store,
  int focus = -1,
  bool focusVisible = true
);
void paintClusterOnlyScene(
  glim::paint::Context& ctx,
  glim::toolkit::host::LayerFeed& feed,
  Store& store
);

namespace {

Engine engineFor(Store& store) {
  Engine e = host::measureEngine();
  e.store = store;
  return e;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 2 && std::string(argv[1]) == "--paint") {
    bool perspective = false;
    for (int i = 1; i < argc; ++i)
      if (std::string(argv[i]) == "--perspective") perspective = true;
    glim::paint::Context ctx;
    ctx.setSize({1168, 784});
    ctx.beginFrame();
    host::LayerFeed feed;
    Store app;
    if (perspective) {
      Engine e = host::measureEngine();
      e.store = app;
      Block screen = mapPerspective({.width = 1168.f, .height = 784.f});
      Box box = e.place(screen, {0, 1168, 0, 784});
      host::paintScreen(ctx, screen, box, e, feed, -1, false);
    } else if (argc > 3 && std::string(argv[3]) == "--cluster-only") {
      paintClusterOnlyScene(ctx, feed, app);
    } else {
      if (argc > 3) app.state("tab", 0) = std::atoi(argv[3]);
      paintInfotainmentScene(ctx, feed, app);
    }
    ctx.finish();
    auto img = glim::image::Image::rgba8(1168, 784);
    glim::paint::rasterScene(ctx.scene(), 1168, 784, img.rgba.data());
    if (!glim::image::Png::write(argv[2], img)) {
      std::cerr << "could not write " << argv[2] << '\n';
      return EXIT_FAILURE;
    }
    std::cout << "wrote " << argv[2] << '\n';
    return EXIT_SUCCESS;
  }

  Store s;
  // Structural wireframe: single-tab reference cluster.
  {
    std::string d = infotainmentDump(s);
    assert(d.find("SpeedGauge") != std::string::npos);
    assert(d.find("Map3D") != std::string::npos);
    assert(d.find("VSI") != std::string::npos);
    assert(d.find("SysStatus") != std::string::npos);
    assert(d.find("BatteryArc") != std::string::npos);
    assert(d.find("ModeBar") != std::string::npos);
    assert(d.find("NavCard") != std::string::npos);
  }
  // Single-tab cluster is read-only: a11y may be empty (no focusables).
  (void)infotainmentA11yDump(s);

  // Determinism: idle scene renders identically twice.
  {
    glim::paint::Context pctx;
    pctx.setSize({1168, 784});
    pctx.beginFrame();
    host::LayerFeed pfeed;
    Store pstore;
    paintInfotainmentScene(pctx, pfeed, pstore, -1, false);
    pctx.finish();
    glim::image::Image a = glim::image::Image::rgba8(1168, 784);
    glim::paint::rasterScene(pctx.scene(), 1168, 784, a.rgba.data());
    glim::paint::Context qctx;
    qctx.setSize({1168, 784});
    qctx.beginFrame();
    host::LayerFeed qfeed;
    Store qstore;
    paintInfotainmentScene(qctx, qfeed, qstore, -1, false);
    qctx.finish();
    glim::image::Image b = glim::image::Image::rgba8(1168, 784);
    glim::paint::rasterScene(qctx.scene(), 1168, 784, b.rgba.data());
    assert(a.rgba == b.rgba);
  }

  // Record gate: build+place+record under 4ms.
  {
    glim::paint::Context ctx;
    ctx.setSize({1168, 784});
    host::LayerFeed feed;
    Store store;
    const double best = glim::examples::bestRecordMs([&] {
      ctx.beginFrame();
      paintInfotainmentScene(ctx, feed, store, -1, false);
      ctx.finish();
    });
    std::cout << "record " << best << " ms\n";
#ifndef GLIM_COVERAGE
    assert(best < 4.0);
#endif
  }

  // Interaction: single-tab cluster builds (no tab switching).
  {
    Store t;
    Engine te = engineFor(t);
    Block tree = infotainmentTree(rootScope(te));
    Box box = te.place(tree, {0, 1168, 0, 1600});
    std::ostringstream os;
    dump(os, tree, box, te);
    std::string dd = os.str();
    assert(dd.find("SpeedGauge") != std::string::npos);
    assert(dd.find("BatteryArc") != std::string::npos);
  }

  // Bridge ops: single-tab CLUSTER is opaque + Plus needle glow. Focus
  // ring last.
  {
    auto paintRec = [&](Store& s) {
      Engine e = engineFor(s);
      Block screen = infotainmentTree(rootScope(e));
      Box dbox = place(screen, {0, 1168, 0, 784});
      OpsRecorder rec;
      host::LayerFeed stubFeed;
      stubFeed.id = 7;
      stubFeed.foreign = true;
      {
        Engine pe;
        paintTree(screen, dbox, pe, rec, stubFeed);
        paintFocusRing(screen, dbox, 0, rec);
      }
      return std::make_tuple(std::move(screen), dbox, std::move(rec));
    };
    auto findOp =
      [&](const std::vector<Op>& ops, Op::Kind k, int nth = 0) -> int {
      int seen = 0;
      for (std::size_t i = 0; i < ops.size(); ++i)
        if (ops[i].kind == k && seen++ == nth)
          return static_cast<int>(i);
      return -1;
    };
    // CLUSTER: rounded glass plates with toolkit drop shadows, rotated
    // needle, arc ring, polyline frames/route, 3D-capable map backdrop
    // (null provider => procedural fallback).
    Store cs;
    auto [cscreen, cdbox, crec] = paintRec(cs);
    assert(findOp(crec.ops, Op::Kind::PushBlend, 0) >= 0);
    assert(findOp(crec.ops, Op::Kind::PushRotate, 0) >= 0);
    assert(findOp(crec.ops, Op::Kind::FillChamfer, 0) >= 0);
    assert(findOp(crec.ops, Op::Kind::Arc, 0) >= 0);
    assert(findOp(crec.ops, Op::Kind::Polyline, 0) >= 0);
    // Floating cards are frosted glass over the 3D map backdrop.
    assert(findOp(crec.ops, Op::Kind::PushGlass, 0) >= 0);
    // Six cards and the mode bar. The dial lens is not a card.
    int shadows = 0;
    for (const Op& o : crec.ops)
      if (o.kind == Op::Kind::PushShadow) {
        assert(
          o.sigma == 24.f && o.soy == 8.f &&
          o.shadowColor == 0x000000e6u
        );
        ++shadows;
      }
    assert(shadows == 7);
    // First-class effect: needle/crown glows ride PushEffect (Glow),
    {
      int ei = findOp(crec.ops, Op::Kind::PushEffect, 0);
      assert(ei >= 0);
      assert(crec.ops[static_cast<std::size_t>(ei)].effect == 0);
      int e1 = findOp(crec.ops, Op::Kind::PushEffect, 1);
      assert(e1 >= 0);
    }

    std::vector<std::uint8_t> bytes;
    host::encodeOps(crec.ops, bytes);
    std::vector<Op> back;
    assert(host::decodeOps(bytes.data(), bytes.size(), back));
    assert(back == crec.ops);

    glim::paint::Context ctx;
    ctx.setSize({1168, 784});
    ctx.beginFrame();
    {
      host::LayerFeed feed;
      Engine pe2;
      host::paintScreen(ctx, cscreen, cdbox, pe2, feed, 0);
    }
    ctx.finish();
    int glassGroups = 0, glyphRuns = 0, shadowGroups = 0;
    std::function<void(const glim::paint::Group&)> walkG =
      [&](const glim::paint::Group& g) {
        glim::paint::visitGroup(
          g,
          [&](const glim::paint::Shape& s) {
            if (std::holds_alternative<glim::paint::GlyphRun>(s))
              ++glyphRuns;
          },
          [&](const glim::paint::Group& c) {
            if (glim::paint::hasGlass(c)) ++glassGroups;
            if (glim::paint::hasShadow(c)) {
              assert(c.params.opacity == 0.f);
              ++shadowGroups;
            }
            walkG(c);
          }
        );
      };
    walkG(ctx.scene().root);
    // Six cards, the mode bar and the dial lens. Shadows are the six
    // cards plus the mode bar; the caster does not cover the map.
    assert(glassGroups == 8);
    assert(shadowGroups == 7);
    assert(glyphRuns > 0);
  }

  // Native GPU-effect plumbing for custom elements (backend-
  // independent: op + archive level). PushRotate carries pivot +
  // degrees through Emit -> Ops -> Codec; FillRadial carries a
  // compositor radial gradient. NOTE: encoder rasterization of
  // rotated quads is still AABB-baked (Merge::transformRect), so
  // widgets keep dot-tessellated needles until encoders catch up.
  {
    Block tree = row(
      rotate({.angleDeg = 30.f, .cx = 10.f, .cy = 20.f}), lines("Rotor")
    );
    Engine e;
    Box b = place(tree, {0, 720, 0, 480});
    OpsRecorder rec;
    host::LayerFeed feed;
    paintTree(tree, b, e, rec, feed);
    int ri = -1, pops = 0;
    for (std::size_t i = 0; i < rec.ops.size(); ++i) {
      if (rec.ops[i].kind == Op::Kind::PushRotate) ri = (int)i;
      if (rec.ops[i].kind == Op::Kind::Pop) ++pops;
    }
    assert(ri >= 0);
    assert(rec.ops[ri].angleDeg == 30.f);
    assert(rec.ops[ri].rcx == 10.f && rec.ops[ri].rcy == 20.f);
    assert(pops >= 1);  // rotate scope balanced
    std::vector<std::uint8_t> bytes;
    host::encodeOps(rec.ops, bytes);
    std::vector<Op> back;
    assert(host::decodeOps(bytes.data(), bytes.size(), back));
    assert(back == rec.ops);
  }

  // Gradient-chamfer: compositor path emits gradient quads (not solid).
  {
    glim::paint::Context ctx;
    ctx.setSize({200, 100});
    ctx.beginFrame();
    glim::paint::GradientStop stops[2] = {
      {0.f, glim::Color{255, 106, 0, 255}},
      {1.f, glim::Color{224, 78, 0, 255}},
    };
    ctx.setFill(
      glim::paint::Matter::linearGradient({0, 0}, {200, 0}, stops, 2)
    );
    ctx.fillChamfer({{10, 10}, {180, 80}}, 14.f);
    ctx.finish();
    auto pkt = glim::paint::encode(ctx.scene(), 1.f);
    assert(!pkt.gradients.empty());
    assert(pkt.quads.empty());
  }

  // Negative arc sweep normalizes to positive equivalent.
  {
    glim::paint::Context ctx;
    ctx.setSize({200, 200});
    ctx.beginFrame();
    ctx.setFillColor(0xff6a00ff);
    glim::paint::Arc a;
    a.center = {100, 100};
    a.radius = 50;
    a.thickness = 8;
    a.startDeg = 0;
    a.sweepDeg = -90;
    ctx.arc(a);
    ctx.finish();
    bool found = false;
    for (const auto& sh : ctx.scene().root.shapes) {
      if (const auto* ar = std::get_if<glim::paint::Arc>(&sh)) {
        assert(ar->sweepDeg == 90.f);
        assert(ar->startDeg == -90.f);
        found = true;
      }
    }
    assert(found);
  }

  // CPU-projection 3D map (cross-platform half of the dual strategy):
  // longitudinal + transverse streets converge via mapProject and emit
  // only existing 2D ops (polyline/Plus/gradient) — no new paint op,
  // no shader, identical on Metal/Vulkan/WebGPU/CPU. Deterministic.
  {
    Store s;
    Engine e = engineFor(s);
    Block tree = mapPerspective({.width = 400.f, .height = 300.f});
    Box b = place(tree, {0, 400, 0, 300});
    OpsRecorder r1, r2;
    host::LayerFeed feed;
    paintTree(tree, b, e, r1, feed);
    paintTree(tree, b, e, r2, feed);
    assert(r1.ops == r2.ops);
    int polys = 0, plus = 0;
    for (const auto& op : r1.ops) {
      if (op.kind == Op::Kind::Polyline) ++polys;
      if (op.kind == Op::Kind::PushBlend) ++plus;
    }
    // 11 longitudinal + 12 transverse + 14 shorts + 1 route.
    assert(polys >= 35);
    assert(plus >= 1);
    std::vector<std::uint8_t> bytes;
    host::encodeOps(r1.ops, bytes);
    std::vector<Op> back;
    assert(host::decodeOps(bytes.data(), bytes.size(), back));
    assert(back == r1.ops);
  }

  std::cout << "infotainment_test ok\n";
  return EXIT_SUCCESS;
}
