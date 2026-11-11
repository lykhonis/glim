// Infotainment / flying-EV cockpit: pure digital screen UI.
//
// Surfaces (one window, no bezels/knobs):
//   - CLUSTER (default, matches paQys.jpg): 3-column reference layout
//     with subtle street-map behind the center gauge — battery arc,
//     altitude + terrain spark, systems rows, 0-300 speedo + target
//     pill, mode bar, VSI, navigation, environment.
//   - Single tab: CLUSTER only. Other console tabs removed for
//     resize + pixel-perfect focus.
//
// Scene stays inside Host/Graphics/Toolkit: fills, text, gradients,
// Plus glow, GlassTag plates. No foreign surfaces, no custom shaders,
// no side-channel drawing — runs on Metal/Vulkan/WebGPU/CPU.
//
// Glass ordering (GPU assert): [under][Plus][glass][overlay shapes].

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Flight.h"
#include "InfoTheme.h"
#include "Infotainment.h"
#include "glim/toolkit.h"
#include "glim/toolkit/host/Host.h"
#include "glim/toolkit/host/Paint.h"
#include "glim/toolkit/host/Probe.h"
#include "widgets/Cluster.h"
#include "widgets/Map3D.h"
#include "widgets/Pinned.h"
#include "widgets/SpeedGauge.h"

using namespace glim::toolkit;

namespace glim {
namespace toolkit {

namespace {

// ---- Reference cluster (paQys.jpg) ----

// Flight clock (seconds), advanced by followFlightClock; until a frame
// steps it, builds read 0, the reference frame.
constexpr const char* kFlightClock = "flight/t";

#ifndef GLIM_GOLDEN  // the golden build has no run loop to drive it
// Mirror the store's frame time into the flight clock while the watch
// lives.
Store::Watch followFlightClock(Store& store) {
  const State<float> clock = store.state(kFlightClock, 0.f);
  return store.watchFrame([clock](float now) {
    clock.set(now);
    return true;
  });
}
#endif

Flight flightOf(const Scope& app) {
  return flightAt(app.store ? *app.state(kFlightClock, 0.f) : 0.f);
}

Block clusterBoard(Scope app) {
  const Flight d = flightOf(app);
  // Columns hug the window edges, the dial and mode bar stay centered.
  // Shadows, then every glass backing, then children: plates stay
  // consecutive and card content stays crisp.
  const GlassTag card = fui::cardGlass();
  const GlassTag control = fui::controlGlass();
  // Toolkit drop shadow under each floating plate. The dial lens stays
  // unshadowed so the gauge face does not pick up a ring.
  const ShadowTag lift = shadow(24.f, 0.f, 8.f, 0x000000e6u);
  constexpr Anchor L = Anchor::Start, C = Anchor::Center,
                   R = Anchor::End;
  return pinned(
      {.width = 1168.f,
       .height = 784.f,
       .pins =
           {{{0, 0, 300, 784}, clusterChrome(false), L},
            {{880, 0, 288, 784}, clusterChrome(true), R},
            {{324, 70, 520, 520},
             speedGauge(
                 {.value = d.speedKmh / 300.f,
                  .speedKmh = d.speedKmh,
                  .targetKmh = d.targetKmh,
                  .needleKmh = d.needleKmh,
                  .speedRoll = d.speedRoll}
             ),
             C,
             fui::dialGlass(),
             {325.5f, 76.f, 516.f, 516.f}},
            {{18, 21, 238, 273},
             batteryArc({.level = d.batteryLevel, .rangeKm = d.rangeKm}
             ),
             L,
             card,
             {},
             lift},
            {{22, 309, 236, 204},
             altitudeCard({.altM = d.altM, .targetM = d.targetAltM}),
             L,
             card,
             {},
             lift},
            {{21, 529, 283, 193}, sysStatus({}), L, card, {}, lift},
            {{914, 21, 237, 303},
             vsi({.rate = d.vsiRate}),
             R,
             card,
             {},
             lift},
            {{913, 339, 233, 179},
             navCard(
                 {.distanceKm = d.distanceKm, .headingDeg = d.headingDeg
                 }
             ),
             R,
             card,
             {},
             lift},
            {{895, 532, 250, 198}, envCard({}), R, card, {}, lift},
            {{323, 663, 543, 71}, modeBar({}), C, control, {}, lift}}}
  );
}

// Chromeless reference frame: no tabs, no section header — 1:1
// against paQys.jpg. Product chrome stays in infotainmentTree.
Block clusterOnlyTree(Scope app) {
  // 3D-capable backdrop matched to the reference aspect (1168x784):
  // null provider paints the procedural mesh fallback. A live session
  // wires a Map3DProvider via ensureMap3DFeed in the run-loop record
  // lambda (foreign in-scene texture under glass), or opts into CPU
  // projection with {.perspective = true} (cross-platform, no native
  // texture); see Map3D.h/MapPerspective.h.
  Block bg = map3d(
      {.width = 1168.f,
       .height = 784.f,
       .perspective = true,
       .city = true,
       .travel = flightOf(app).travel}
  );
  return stack(std::move(bg), clusterBoard(app));
}

}  // namespace

Block infotainmentTree(Scope app) {
  // Single-tab product: cluster only (paQys.jpg reference).
  // Console tabs removed — cluster pixel-perfect is the focus.
  return infoThemed(clusterOnlyTree(app));
}

}  // namespace toolkit
}  // namespace glim

std::string dumpTree(Block tree, Constraints c) {
  std::ostringstream os;
  Engine e = host::measureEngine();
  Box box = e.place(tree, c);
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree, box, e);
  return os.str();
}

std::string infotainmentDump(Store& store) {
  Engine e = host::measureEngine();
  e.store = store;
  return dumpTree(infotainmentTree(rootScope(e)), {0, 1168, 0, 784});
}

std::string infotainmentA11yDump(Store& store) {
  Engine e = host::measureEngine();
  e.store = store;
  Block tree = infotainmentTree(rootScope(e));
  Box box = e.place(tree, {0, 1168, 0, 784});
  std::ostringstream os;
  dumpA11y(os, a11yNodes(tree, box, e));
  return os.str();
}

void paintInfotainmentScene(
    glim::paint::Context& ctx,
    host::LayerFeed& feed,
    Store& store,
    int focus = -1,
    bool focusVisible = true
) {
  ensureCityPlate(ctx, store);
  Engine e = host::measureEngine();
  e.store = store;
  Block screen = infotainmentTree(rootScope(e));
  Box box = e.place(screen, {0, ctx.size().x, 0, ctx.size().y});
  host::paintScreen(ctx, screen, box, e, feed, focus, focusVisible);
}

void paintClusterOnlyScene(
    glim::paint::Context& ctx, host::LayerFeed& feed, Store& store
) {
  ensureCityPlate(ctx, store);
  Engine e = host::measureEngine();
  e.store = store;
  Block screen = clusterOnlyTree(rootScope(e));
  Box box = e.place(screen, {0, ctx.size().x, 0, ctx.size().y});
  host::paintScreen(ctx, screen, box, e, feed, -1, false);
}

#ifndef GLIM_GOLDEN
#if defined(__ANDROID__)
extern "C" int glim_app_main() {
  int argc = 0;
  char** argv = nullptr;
#else
int main(int argc, char** argv) {
#endif
  const host::E2eFlags flags = host::parseE2e(argc, argv);
  if (flags.help) {
    std::cout << "usage: glim-infotainment " << host::kE2eUsage
              << "\n  [--cluster-only]\n";
    return 0;
  }
  bool clusterOnly = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--cluster-only") clusterOnly = true;
  }
  {
    if (flags.any()) {
      host::TestApp t;
      t.w = 1168;
      t.h = 784;
      t.build = [clusterOnly](const Engine& e) {
        return clusterOnly ? clusterOnlyTree(rootScope(e))
                           : infotainmentTree(rootScope(e));
      };
      t.prepare = ensureCityPlate;
      // `step` advances the flight; frames stay at t = 0 until then.
      const Store::Watch flight = followFlightClock(t.store);
      return host::runE2e(t, flags);
    }
  }
  Store store;
  Screen screen;
  // Live flight: advance the drive clock every vsync (repaints follow).
  const Store::Watch flight = followFlightClock(store);
  return host::run(
      {.title = "GLIM // Flying EV Infotainment",
       .width = 1168,
       .height = 784,
       .customTitlebar = true,
       .drive = flags.drive,
       .driveTrace = flags.driveTrace,
       .startup = flags.startup,
       .prepare = ensureCityPlate},
      screen,
      store,
      host::measureEngine(),
      [](const Engine& e) { return infotainmentTree(rootScope(e)); },
      nullptr
  );
}
#endif
