#include "Scene.h"
#include "SmartHome.h"

#include "RecordBudget.h"

#include <glim/image/Golden.h>
#include <glim/image/Image.h>
#include <glim/image/Png.h>
#include <glim/paint/Context.h>
#include <glim/paint/Scene.h>
#include <glim/paint/Software.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "glim/toolkit.h"
#include "glim/toolkit/host/Codec.h"
#include "glim/toolkit/host/Host.h"
#include "glim/toolkit/host/Paint.h"
#include "glim/toolkit/host/Telemetry.h"
#include "widgets/Glide.h"
#include "widgets/HomeTheme.h"
#include "widgets/Switch.h"

using namespace glim::toolkit;

namespace {

// Zero-codec peer: a brand-new kind never seen by Ops.h rides the
// generic entry codec untouched.
struct Probe {
  const char* kind() const { return "Probe"; }
  Box place(Constraints c, const Engine&) const {
    return glim::toolkit::detail::resolveSize(10.0f, 10.0f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push("Probe", box, "probe", "mark", 3);
  }
};

// Paint-hook peer: custom paint() for every sink (pixels on
// interpreters, ops on recorders). No record() at all.
struct CustomPainted {
  const char* kind() const { return "CustomPainted"; }
  Box place(Constraints c, const Engine&) const {
    return glim::toolkit::detail::resolveSize(10.0f, 10.0f, c);
  }
  void paint(const Box& b, AnySink& s, const Engine&) const {
    s.stroke(b, 0, 1.0f, 0xff0000ff);
  }
};

Engine engineFor(Store& store) {
  Engine e = host::measureEngine();
  e.store = store;
  return e;
}

std::string dumpOf(
    Store& store,
    float w,
    float h,
    const glim::home::SensorLog& log = glim::home::demoLog()
) {
  Engine e = engineFor(store);
  Block tree = smartHomeTree(rootScope(e), log);
  Box box = e.place(tree, {0, w, 0, h});
  std::ostringstream os;
  dump(os, tree, box, e);
  return os.str();
}

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

// Distinct card columns at a window width: the grid reflows.
std::size_t cardColumns(float width) {
  Store s;
  Engine e = engineFor(s);
  Block tree = smartHomeTree(rootScope(e));
  Box box = e.place(tree, {0, width, 0, 760});
  std::set<int> xs;
  walk(
      tree,
      box,
      e,
      [&](const Block& n, const Box& b, const Engine&) {
        if (auto c = n.get<Custom>())
          if (c->get().as<Pane>()) xs.insert(static_cast<int>(b.x));
        return true;
      },
      [](const Block&, const Box&, const Engine&) {}
  );
  return xs.size();
}

glim::image::Image paintSmartHome(Store& app, int height) {
  glim::paint::Context ctx;
  ctx.setSize({1240.0f, static_cast<float>(height)});
  ctx.beginFrame();
  host::LayerFeed feed;
  paintSmartHomeScene(ctx, feed, app);
  ctx.finish();
  auto img = glim::image::Image::rgba8(1240, height);
  glim::paint::rasterScene(ctx.scene(), 1240, height, img.rgba.data());
  return img;
}

bool press(const Block& tree, const Box& box, Box at, const Engine& e) {
  return dispatchPointer(
      tree,
      box,
      at.x + 1,
      at.y + 1,
      Event::borrow(PointerDown{at.x + 1, at.y + 1}),
      e
  );
}

}  // namespace

int main(int argc, char** argv) {
  // Default scene vs the PNG golden (L2): --golden
  // path/to/smarthome.png.
  if (argc > 2 && std::string(argv[1]) == "--golden") {
    Store app;
    return glim::image::matchCpuGolden(
               paintSmartHome(app, 760), argv[2], argv[0], "smarthome"
           )
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
  }
  // Headless eyeball: --paint out.png [live] [light] [height] renders
  // the full scene through the software rasterizer, no window needed.
  if (argc > 2 && std::string(argv[1]) == "--paint") {
    const int paintH =
        argc > 5 ? std::max(200, std::atoi(argv[5])) : 760;
    Store app;
    if (argc > 3) app.state("live", true) = (std::atoi(argv[3]) != 0);
    if (argc > 4) app.state("theme", true) = (std::atoi(argv[4]) != 0);
    if (!glim::image::Png::write(
            argv[2], paintSmartHome(app, paintH)
        )) {
      std::cerr << "could not write " << argv[2] << '\n';
      return EXIT_FAILURE;
    }
    std::cout << "wrote " << argv[2] << '\n';
    return EXIT_SUCCESS;
  }

  // Shell-to-toolkit key mapping: Dpads move, Enter/Center activate,
  // everything else falls through to built-in behavior.
  using glim::toolkit::host::mapKey;
  assert(mapKey(glim::shell::Key::DpadLeft) == Key::Left);
  assert(mapKey(glim::shell::Key::DpadRight) == Key::Right);
  assert(mapKey(glim::shell::Key::DpadUp) == Key::Up);
  assert(mapKey(glim::shell::Key::DpadDown) == Key::Down);
  assert(mapKey(glim::shell::Key::Enter) == Key::Activate);
  assert(mapKey(glim::shell::Key::DpadCenter) == Key::Activate);
  assert(!mapKey(glim::shell::Key::Escape));
  assert(!mapKey(glim::shell::Key::Unknown));

  // Home domain: plain values in, display strings out.
  {
    using namespace glim::home;
    assert(rooms().size() == 3);
    assert(levelText(Kind::Light, 0.8f) == "80%");
    assert(levelText(Kind::Climate, 0.5f) == "22.0 C");
    assert(levelText(Kind::Speaker, 0.3f) == "Volume 30%");
    assert(levelText(Kind::Blinds, 0.6f) == "Open 60%");
    Device off{"x", "X", Kind::Light, false, 0.9f};
    assert(statusText(off) == "Off");
    off.on = true;
    assert(statusText(off) == "90%");
    assert(activeCount({}) == 0);
  }

  // Sensors: frame telemetry read as a home. Frames fold into
  // half-second buckets; readings only use closed ones.
  {
    using namespace glim::home;
    assert(
        band(0.2f) == "Low" && band(0.5f) == "Medium" &&
        band(0.9f) == "High"
    );
    assert(
        feedStatus(true) == "All devices online" &&
        feedStatus(false) == "Updates paused"
    );
    // Steady 16 ms frames with 3 ms of work for two seconds: a
    // half-loaded, 62.5 fps, noise-free house.
    SensorLog log;
    for (int i = 0; i < 125; ++i)
      log = logSample(std::move(log), {i * 0.016f, 16.0f, 3.0f});
    assert(log.closed.size() == 3 && log.open.frames > 0);
    const Readings r = readings(log);
    assert(r.energy == 0.5f && percentText(r.energy) == "50%");
    assert(
        std::fabs(r.fps - 62.5f) < 1e-3f &&
        kwhText(r.solarKwh) == "12.5"
    );
    assert(
        r.indoorC == 22.5f && temperatureText(r.indoorC) == "22.5 C"
    );
    assert(
        r.wifiBars == 4 && wifiStatus(r, true) == "Excellent signal"
    );
    assert(
        signalBars(r, false) == 0 &&
        wifiStatus(r, false) == "Disconnected"
    );
    assert(
        latencyText(r, true) == "16.0 ms" &&
        jitterText(r, true) == "0.0 ms"
    );
    // Readings hold still inside a bucket: frames that land in the
    // open bucket change nothing until it closes.
    SensorLog more = logSample(
        log, {log.open.index * kBucketSec + 0.01f, 40.0f, 6.0f}
    );
    assert(
        readings(more).fps == r.fps && readings(more).energy == r.energy
    );
    more = logSample(
        std::move(more),
        {(log.open.index + 1) * kBucketSec, 16.0f, 3.0f}
    );
    assert(readings(more).energy > r.energy);
    // Input paints between vsyncs split a frame in two: the short
    // front half folds into the frame that ends it.
    {
      SensorLog split;
      for (int i = 0; i < 125; ++i) {
        split = logSample(
            std::move(split), {i * 0.016f - 0.004f, 2.0f, 1.0f}
        );
        split = logSample(std::move(split), {i * 0.016f, 14.0f, 2.0f});
      }
      const Readings sr = readings(split);
      assert(
          std::fabs(sr.fps - 62.5f) < 1e-3f && sr.jitterMs < 1e-3f &&
          sr.energy == 0.5f
      );
    }
    // Nothing closed yet: zero readings, empty curve.
    const Readings none = readings(logSample({}, {0.0f, 16.0f, 3.0f}));
    assert(
        none.energy == 0.0f && none.solarCurve.empty() &&
        none.wifiBars == 0
    );
    // Pacing noise degrades the signal.
    SensorLog noisy;
    for (int i = 0; i < 120; ++i)
      noisy = logSample(
          std::move(noisy), {i * 0.017f, i % 2 ? 10.0f : 24.0f, 3.0f}
      );
    assert(
        readings(noisy).wifiBars == 1 &&
        readings(noisy).wifiQuality == "Weak"
    );
    // The log keeps a bounded window; the curve maps it against its
    // peak.
    const SensorLog& demo = demoLog();
    assert(demo.closed.size() == kLogBuckets);
    const Readings d = readings(demo);
    assert(d.solarCurve.size() == kLogBuckets);
    const auto [lo, hi] =
        std::minmax_element(d.solarCurve.begin(), d.solarCurve.end());
    assert(*lo >= 0.2f && *hi == 0.9f && *lo < *hi);
    // Lights and the thermostat read from device values.
    Device ceiling = rooms()[0].devices[0];
    assert(percentText(brightness({ceiling})) == "80%");
    ceiling.on = false;
    assert(brightness({ceiling}) == 0.0f && brightness({}) == 0.0f);
    Device thermostat = rooms()[0].devices[2];
    assert(climateText(21.6f, thermostat) == "Holding at 21.4 C");
    assert(
        climateText(18.0f, thermostat) == "Heating to 21.4 C, 14 min"
    );
    assert(
        climateText(24.0f, thermostat) == "Cooling to 21.4 C, 11 min"
    );
    thermostat.on = false;
    assert(climateText(21.6f, thermostat) == "Thermostat off");
  }

  // The adapter turns host telemetry into samples, and the page shows
  // them: what the loop measures is what the home reads.
  {
    host::FrameTelemetry t{};
    t.time = 1.25f;
    t.frameMs = 16.0f;
    t.buildMs = 0.5f;
    t.recordMs = 1.0f;
    t.encodeMs = 1.5f;
    const glim::home::FrameSample sample = frameSample(t);
    assert(
        sample.time == 1.25f && sample.frameMs == 16.0f &&
        sample.workMs == 3.0f
    );
    glim::home::SensorLog log;
    for (int i = 0; i < 125; ++i) {
      t.time = i * 0.016f;
      log = glim::home::logSample(std::move(log), frameSample(t));
    }
    Store live;
    const std::string d = dumpOf(live, 1240, 1100, log);
    assert(
        has(d, "50%") && has(d, "Medium") && has(d, "12.5 kWh") &&
        has(d, "22.5 C")
    );
    assert(has(d, "Excellent signal") && has(d, "16.0 ms"));
  }

  Store s;
  {
    const std::string d = smartHomeDump(s);
    assert(
        has(d, "Backdrop") && has(d, "Grid") && has(d, "Card") &&
        !has(d, "Nav")
    );
    assert(
        has(d, "Glim Home") && has(d, "All devices online") &&
        has(d, "Energy")
    );
    assert(
        has(d, "Meter") && has(d, "Gauge") && has(d, "AreaChart") &&
        has(d, "TickRuler")
    );
    assert(has(smartHomeFocusDump(s), kFocusRingKind));
    // Every control carries a spoken name, and named controls that
    // draw arcs and polylines paint through the marked sink.
    const std::string a11y = smartHomeA11yDump(s);
    for (const char* name :
         {"Wi-Fi",
          "Thermostat",
          "Target temperature",
          "Ceiling lights",
          "Brightness",
          "Live updates",
          "Light mode"})
      assert(has(a11y, name));
  }
  // Determinism: the scene renders identically twice.
  {
    auto render = [] {
      glim::paint::Context ctx;
      ctx.setSize({1024, 640});
      ctx.beginFrame();
      host::LayerFeed feed;
      Store store;
      paintSmartHomeScene(ctx, feed, store, -1, false);
      ctx.finish();
      glim::image::Image img = glim::image::Image::rgba8(1024, 640);
      glim::paint::rasterScene(ctx.scene(), 1024, 640, img.rgba.data());
      return img.rgba;
    };
    assert(render() == render());
  }

  // Record gate: tree build + place + record stays under 4 ms. The
  // host loop reuses its Store, LayerFeed and Context, so the gate
  // does too; min-of-N after warmup filters scheduling noise.
  {
    glim::paint::Context ctx;
    ctx.setSize({1024, 640});
    host::LayerFeed feed;
    Store store;
    const double best = glim::examples::bestRecordMs([&] {
      ctx.beginFrame();
      paintSmartHomeScene(ctx, feed, store, -1, false);
      ctx.finish();
    });
    std::cout << "record " << best << " ms" << std::endl;
#ifndef GLIM_COVERAGE
    assert(best < 4.0);
#endif
  }

  // The grid reflows with the window: one column when narrow, three
  // (energy + Wi-Fi stacked, thermostat, light) when wide.
  assert(cardColumns(560) == 1);
  assert(cardColumns(800) == 2);
  assert(cardColumns(1240) == 2);
  assert(cardColumns(1920) == 2);

  // Glide: a reading eases to each new goal; rebuilding mid-flight with
  // the same goal keeps the curve, and it settles exactly.
  {
    Store gs;
    Engine ge = engineFor(gs);
    Scope sc = rootScope(ge);
    assert(glide(sc, "g", 0.0f) == 0.0f && !gs.valuesLive());
    assert(glide(sc, "g", 1.0f) == 0.0f && gs.valuesLive());
    gs.stepValues(10.0f);  // anchors
    gs.stepValues(10.0f + kGlideSpec.dur * 0.5f);
    const float mid = glide(sc, "g", 1.0f);
    assert(mid > 0.0f && mid < 1.0f);
    gs.stepValues(10.0f + kGlideSpec.dur * 0.75f);
    assert(glide(sc, "g", 1.0f) > mid);
    gs.stepValues(10.0f + kGlideSpec.dur * 1.01f);
    assert(glide(sc, "g", 1.0f) == 1.0f && !gs.valuesLive());

    // Coast: a big jump keeps easing past the time a nudge needs.
    assert(coast(sc, "c", 0.0f) == 0.0f);
    assert(coast(sc, "c", 0.5f) == 0.0f);  // half the range
    gs.stepValues(40.0f);                  // anchors
    gs.stepValues(40.0f + kGlideSpec.dur * 1.5f);
    assert(coast(sc, "c", 0.5f) < 0.5f && gs.valuesLive());
    gs.stepValues(40.0f + kGlideSpec.dur + 0.5f * 1.0f + 0.01f);
    assert(coast(sc, "c", 0.5f) == 0.5f && !gs.valuesLive());
    assert(coast(sc, "c", 0.51f) == 0.5f);  // a nudge: brisk
    gs.stepValues(50.0f);
    gs.stepValues(50.0f + kGlideSpec.dur + 0.011f);
    assert(coast(sc, "c", 0.51f) == 0.51f && !gs.valuesLive());

    // Slide: each new chart head pushes the curve one point right,
    // then it scrolls back at a steady rate; jumps cap at the lead.
    assert(slide(sc, "s", 10, 0.5f, 2.0f) == 0.0f);
    assert(slide(sc, "s", 11, 0.5f, 2.0f) == 1.0f);
    gs.stepValues(20.0f);
    gs.stepValues(20.0f + 0.275f);
    assert(std::fabs(slide(sc, "s", 11, 0.5f, 2.0f) - 0.5f) < 1e-3f);
    assert(std::fabs(slide(sc, "s", 12, 0.5f, 2.0f) - 1.5f) < 1e-3f);
    assert(slide(sc, "s", 40, 0.5f, 2.0f) == 2.0f);
    assert(slide(sc, "s", 3, 0.5f, 2.0f) == 0.0f);
    gs.stepValues(30.0f);
    gs.stepValues(31.0f);
    assert(!gs.valuesLive());
  }

  // Touch end to end. Focus order: Wi-Fi switch, thermostat switch and
  // ruler, ceiling switch and dial, then Live and Light in the header.
  // Gestures run like the host: a fresh tree per event, one Screen.
  {
    Store t;
    Engine te = engineFor(t);
    Screen scr;
    auto at = [&](int) {
      Block tree = smartHomeTree(rootScope(te));
      Box box = te.place(tree, {0, 1240, 0, 1100});
      assert(focusables(tree, box, te).size() == 7);
      return std::make_pair(tree, box);
    };
    auto tap = [&](int i) {
      auto [tree, box] = at(i);
      return press(tree, box, focusBox(tree, box, i, te), te);
    };
    auto boxOf = [&](int i) {
      auto [tree, box] = at(i);
      return focusBox(tree, box, i, te);
    };
    // Down at the first point, pressed moves through the rest, up at
    // the last.
    auto gesture = [&](std::vector<std::pair<float, float>> pts) {
      {
        auto [tree, box] = at(0);
        assert(scr.pointerDown(
            tree, box, pts[0].first, pts[0].second, 1, te
        ));
      }
      for (std::size_t k = 1; k < pts.size(); ++k) {
        auto [tree, box] = at(0);
        scr.pointerMove(
            tree, box, pts[k].first, pts[k].second, true, 1, te
        );
      }
      auto [tree, box] = at(0);
      scr.pointerUp(tree, box, pts.back().first, pts.back().second, te);
    };
    float clock = 0.0f;  // the pump's time only moves forward
    auto settle = [&] {
      t.stepValues(clock += 10.0f);
      t.stepValues(clock += 10.0f);
    };
    auto celsiusNow = [&] {
      return glim::home::celsius(
          *t.state("home/living/climate/level", 0.0f)
      );
    };

    assert(tap(0) && !*t.state("wifi", true));
    assert(has(dumpOf(t, 1240, 1100), "Disconnected"));

    // Thermostat: the ruler is the control. Dragging the scale left
    // brings higher numbers under the needle; the release lands on a
    // half degree. A press that never leaves the slop is a tap.
    const Box ruler = boxOf(2);
    const float unit =
        ruler.w / static_cast<float>(2 * TickRuler::kSpan + 1);
    const float mid = ruler.x + ruler.w * 0.5f,
                my = ruler.y + ruler.h * 0.5f;
    const float c0 = celsiusNow();
    gesture({{mid, my}, {mid - unit, my}, {mid - 2.0f * unit, my}});
    assert(
        std::fabs(
            celsiusNow() - std::round((c0 + 2.0f) * 2.0f) / 2.0f
        ) < 1e-3f
    );
    // Tapping a number goes there.
    const float c1 = celsiusNow();
    gesture({{mid - 3.0f * unit, my}, {mid - 3.0f * unit + 2.0f, my}});
    assert(std::fabs(celsiusNow() - (c1 - 3.0f)) < 1e-3f);
    // The focused ruler steps by half a degree.
    {
      auto [tree, box] = at(0);
      assert(scr.key(tree, box, Key::Right, te));
    }
    assert(std::fabs(celsiusNow() - (c1 - 2.5f)) < 1e-3f);
    // A flick spins on: the release speed carries the scale past the
    // finger, landing on a half degree. Speed comes from Screen's run
    // time, advanced a frame (1/60 s) per move.
    {
      const float frame = 1.0f / 60.0f;
      auto flickLeft = [&](float perMove, int moves, float restBeforeUp
                       ) {
        {
          auto [tree, box] = at(0);
          assert(scr.pointerDown(tree, box, mid, my, 1, te));
        }
        for (int k = 1; k <= moves; ++k) {
          scr.clock.advance(frame);
          auto [tree, box] = at(0);
          scr.pointerMove(
              tree,
              box,
              mid - perMove * static_cast<float>(k),
              my,
              true,
              1,
              te
          );
        }
        scr.clock.advance(restBeforeUp);
        auto [tree, box] = at(0);
        scr.pointerUp(
            tree, box, mid - perMove * static_cast<float>(moves), my, te
        );
      };
      const float rest = celsiusNow();
      // 0.2 unit a frame = 12 units/s: 5 frames drag 1 degree, the
      // release carries 12 * kFlickTau = 3 more.
      flickLeft(0.2f * unit, 5, 0.0f);
      const float spun = celsiusNow();
      assert(std::fabs(spun - (rest + 1.0f + 3.0f)) <= 0.5f + 1e-3f);
      assert(std::fabs(spun * 2.0f - std::round(spun * 2.0f)) < 1e-3f);
      // The same drag held still before letting go just snaps.
      flickLeft(-0.2f * unit, 5, TickRuler::kRestSec * 2.0f);
      assert(std::fabs(celsiusNow() - (spun - 1.0f)) <= 0.5f + 1e-3f);
      // So does a slow drag: half of kFlickMinPx, in pixels, so a wider
      // ruler does not turn it into a flick.
      const float slow = celsiusNow();
      const float slowPx = TickRuler::kFlickMinPx / 60.0f * 0.5f;
      flickLeft(slowPx, 5, 0.0f);
      assert(
          std::fabs(celsiusNow() - (slow + 5.0f * slowPx / unit)) <=
          0.5f + 1e-3f
      );
    }
    // The hero follows the target, and the scale glides behind it.
    settle();
    assert(has(
        dumpOf(t, 1240, 1100),
        "ing to " + glim::home::temperatureText(celsiusNow()) + ", "
    ));
    assert(tap(1) && !*t.state("home/living/climate/on", true));
    assert(has(dumpOf(t, 1240, 1100), "Thermostat off"));

    // Ceiling light: the dial is the control. A press reads the angle
    // around the dial; the readout glides there.
    const Box dial = boxOf(4);
    const Gauge::Dial d = Gauge::dial(dial);
    settle();
    gesture({{d.cx, d.cy - d.r}});
    assert(
        std::fabs(*t.state("home/living/ceiling/level", 0.0f) - 0.5f) <
        1e-3f
    );
    State<float> shownLight = t.state("glide/brightness", 0.0f);
    assert(
        *shownLight == 0.8f && has(dumpOf(t, 1240, 1100), "80%")
    );  // not stepped yet
    t.stepValues(clock += 10.0f);
    t.stepValues(clock += kGlideSpec.dur * 0.5f);
    assert(*shownLight < 0.8f && *shownLight > 0.5f);
    settle();
    assert(*shownLight == 0.5f && !has(dumpOf(t, 1240, 1100), "80%"));
    // Dragging around to the right end turns it all the way up.
    gesture(
        {{d.cx - d.r, d.cy - 4.0f},
         {d.cx, d.cy - d.r},
         {d.cx + d.r, d.cy - 4.0f},
         {d.cx + d.r, d.cy + 20.0f}}
    );
    assert(*t.state("home/living/ceiling/level", 0.0f) == 1.0f);
    settle();
    assert(has(dumpOf(t, 1240, 1100), "100%"));
    assert(tap(3) && !*t.state("home/living/ceiling/on", true));
    settle();
    assert(has(dumpOf(t, 1240, 1100), "0%"));

    // Live switch: the header reports the feed mode.
    assert(tap(5) && !*t.state("live", true));
    assert(has(dumpOf(t, 1240, 1100), "Updates paused"));

    // Theme switch flips the ramp: the backdrop repaints light or dark.
    State<bool> light = t.state("theme", true);
    auto backdropTop = [&] {
      auto [tree, box] = at(0);
      OpsRecorder rec;
      host::LayerFeed stubFeed;
      paintTree(tree, box, te, rec, stubFeed);
      for (const auto& o : rec.ops)
        if (o.kind == Op::Kind::FillGradient) return o.rgba;
      return 0u;
    };
    assert(*light && backdropTop() == ShowLight::kBgTop);
    assert(tap(6));
    assert(!*light && backdropTop() == Show::kBgTop);
  }

  // Paint: one glass plate per card plus the header, the focus ring as
  // the last stroke, and an ops archive that round-trips
  // byte-identically (rejects truncation/magic).
  {
    Store ps;
    Engine pe = engineFor(ps);
    Block screen = smartHomeTree(rootScope(pe));
    Box dbox = place(screen, {0, 1024, 0, 640});
    OpsRecorder rec;
    {
      host::LayerFeed stubFeed;
      paintTree(screen, dbox, pe, rec, stubFeed);
      paintFocusRing(screen, dbox, 0, rec);
    }
    int glassOps = 0;
    for (const auto& o : rec.ops)
      if (o.kind == Op::Kind::PushGlass) ++glassOps;
    assert(glassOps == 6);
    assert(rec.ops.back().kind == Op::Kind::Stroke);

    std::vector<std::uint8_t> bytes;
    host::encodeOps(rec.ops, bytes);
    std::vector<Op> back;
    assert(host::decodeOps(bytes.data(), bytes.size(), back));
    assert(back == rec.ops);
    assert(!host::decodeOps(bytes.data(), bytes.size() - 1, back));
    bytes[3] ^= 0xff;  // corrupt magic
    assert(!host::decodeOps(bytes.data(), bytes.size(), back));

    glim::paint::Context ctx;
    ctx.setSize({1024, 640});
    ctx.beginFrame();
    {
      host::LayerFeed feed;
      host::paintScreen(ctx, screen, dbox, pe, feed, 0);
    }
    ctx.finish();
    int glyphRuns = 0;
    int glassGroups = 0;
    std::function<void(const glim::paint::Group&)> walkGroups =
        [&](const glim::paint::Group& g) {
          glim::paint::visitGroup(
              g,
              [&](const glim::paint::Shape& sh) {
                if (std::holds_alternative<glim::paint::GlyphRun>(sh))
                  ++glyphRuns;
              },
              [&](const glim::paint::Group& c) {
                if (glim::paint::hasGlass(c)) ++glassGroups;
                walkGroups(c);
              }
          );
        };
    walkGroups(ctx.scene().root);
    std::cout << "glass groups " << glassGroups << "\n";
    assert(glassGroups == 6);
    assert(glyphRuns > 0);
  }

  // Wheel scrolls the page when it overflows a short window.
  {
    Store ws;
    Engine we = engineFor(ws);
    Block tree = smartHomeTree(rootScope(we));
    Box clipped = we.place(tree, {0, 700, 0, 400});
    Screen scr;
    assert(scr.wheel(tree, clipped, 500, 240, 0.0f, 5000.0f, we));
    assert(*ws.state("vscroll", 0.0f) > 0.0f);
  }

  // Live driver mechanics: a never-settling frame step keeps the
  // loop repainting (revision bumps every tick); dropping the watch
  // idles it again.
  {
    Store m;
    State<float> ph = m.state("phase", 0.0f);
    std::uint64_t r0 = m.revision();
    auto w = m.watchFrame([ph](float now) mutable {
      ph = now;
      return true;
    });
    assert(m.valuesLive());
    m.stepValues(1.0f);
    assert(*ph == 1.0f && m.revision() > r0);
    m.stepValues(2.0f);
    assert(*ph == 2.0f);
    w.reset();
    assert(!m.valuesLive());
  }
  // Switch thumb coasts on its Anim: the tap flips the value at
  // once, the position follows without any driver code — the Store
  // pump steps it, each write repainting by itself.
  {
    Store m;
    Scope ms(m, "");
    auto v = ms.state("v", false);
    Anim<float> p{ms, "p", 0.0f};
    Block swt = toggle({.value = v, .pos = p});
    Box sb = place(swt, {0, 720, 0, 480});
    assert(dispatchPointer(
        swt, sb, 26, 14, Event::borrow(PointerDown{26, 14})
    ));
    assert(*v == true && *p == 0.0f && m.valuesLive());
    m.stepValues(0.0f);
    m.stepValues(0.11f);
    assert(*p > 0.0f && *p < 1.0f);
    m.stepValues(1.0f);
    assert(*p == 1.0f && !p.live() && !m.valuesLive());
  }
  // Anim: assignment animates with the latest spec, the Store is
  // the pump, paint reads the display value, logic reads the goal.
  {
    Store m;
    Scope ms(m, "");
    Anim<float> a{ms, "a", 2.0f};
    assert(
        !a.live() && *a == 2.0f && a.target() == 2.0f && !m.valuesLive()
    );
    a = 10.0f;
    assert(
        a.live() && *a == 2.0f && a.target() == 10.0f && m.valuesLive()
    );
    m.stepValues(1.0f);
    m.stepValues(1.11f);
    assert(*a > 2.0f && *a < 10.0f);
    m.stepValues(2.0f);
    assert(*a == 10.0f && !a.live() && !m.valuesLive());
    // Latest assigned spec wins; reassigning then overriding once.
    a.setSpec({.dur = 1.0f, .ease = easeLinear});
    a = 0.0f;
    m.stepValues(3.0f);
    m.stepValues(3.5f);
    assert(*a == 5.0f);
    m.stepValues(4.5f);
    assert(*a == 0.0f && !a.live() && !m.valuesLive());
    // Delay holds the old value, then runs.
    a.set(4.0f, {.dur = 0.2f, .delay = 1.0f});
    m.stepValues(5.0f);
    assert(*a == 0.0f && m.valuesLive());
    m.stepValues(6.5f);
    assert(*a == 4.0f && !a.live() && !m.valuesLive());
    // Snap jumps and cancels.
    a = 8.0f;
    m.stepValues(7.0f);
    a.snap(1.0f);
    assert(
        *a == 1.0f && a.target() == 1.0f && !a.live() && !m.valuesLive()
    );
    // Re-setting the settled value is a no-op: keeps the spec,
    // skips the pump, stays exact.
    a.set(1.0f, {.dur = 1.0f, .ease = easeInOut});
    assert(a.spec().dur == 1.0f && !a.live() && !m.valuesLive());
  }
  // Springs: same assignment semantics, physics instead of curves.
  // Underdamped motion overshoots, then settles exact.
  {
    Store m;
    Scope ms(m, "");
    Anim<float> a{ms, "a", 0.0f};
    a.set(
        10.0f, {.spring = true, .stiffness = 120.0f, .damping = 8.0f}
    );
    assert(a.live() && m.valuesLive());
    float t = 0.0f;
    float peak = 0.0f;
    for (int i = 0; i < 120 && m.valuesLive(); ++i) {
      t += 1.0f / 60.0f;
      m.stepValues(t);
      if (*a > peak) peak = *a;
    }
    assert(peak > 10.0f);
    assert(*a == 10.0f && !a.live() && !m.valuesLive());
  }
  // Chaining: sinks track their source declaratively; waking is
  // automatic even after full settle (per-step poke), cascades
  // compose with no further code.
  {
    Store m;
    Scope ms(m, "");
    Anim<float> lead{ms, "lead", 0.0f};
    Anim<float> trail{ms, "trail", 0.0f};
    Anim<float> tail{ms, "tail", 0.0f};
    trail.follow(lead);
    tail.follow(trail);
    assert(!m.valuesLive());
    lead = 10.0f;
    assert(m.valuesLive());
    float t = 0.0f;
    for (int i = 0; i < 200 && m.valuesLive(); ++i) {
      t += 1.0f / 60.0f;
      m.stepValues(t);
      if (i == 5) assert(*trail < *lead && *tail <= *trail);
    }
    assert(*lead == 10.0f && *trail == 10.0f && *tail == 10.0f);
    assert(!m.valuesLive());
    // Re-arm after full settle: the per-step poke wakes the chain.
    lead = 0.0f;
    assert(m.valuesLive());
    for (int i = 0; i < 200 && m.valuesLive(); ++i) {
      t += 1.0f / 60.0f;
      m.stepValues(t);
    }
    assert(*lead == 0.0f && *trail == 0.0f && *tail == 0.0f);
    assert(!m.valuesLive());
    assert(lead.refs() == 2 && trail.refs() == 2 && tail.refs() == 1);
  }
  // List scroll (direct, tight viewport): content taller than the
  // viewport spills past it, no clip.
  {
    Block scrolled = roll(vertical(), list(8, [](int i) {
                            return lines("item" + std::to_string(i));
                          }));
    Box vbox = place(scrolled, {0, 720, 0, 100});
    assert(vbox.h == 100);
    Rec recr;
    record(scrolled, vbox, recr);
    bool spilled = false;
    for (const auto& e : recr.entries)
      if (e.kind == "Lines" &&
          e.box.y + e.box.h > vbox.y + vbox.h + 0.001f)
        spilled = true;
    assert(spilled);
  }
  // Telemetry hub: pull (ring + last) and push (sink) agree.
  {
    host::TelemetryHub hub(4);
    assert(hub.empty());
    int pushes = 0;
    host::FrameTelemetry last{};
    host::TelemetrySink sink = [&](const host::FrameTelemetry& s) {
      ++pushes;
      last = s;
    };
    host::FrameTelemetry a{};
    a.frameMs = 2.0f;
    a.recordMs = 1.0f;
    a.stats.draws = 7;
    hub.push(a);
    sink(a);
    assert(!hub.empty());
    assert(hub.size() == 1);
    assert(hub.last().stats.draws == 7);
    assert(pushes == 1 && last.frameMs == 2.0f);
    for (int i = 0; i < 6; ++i) {
      host::FrameTelemetry s{};
      s.frameMs = static_cast<float>(i);
      hub.push(s);
    }
    // Capacity caps the ring; history stays oldest-first.
    assert(hub.size() == 4);
    auto hist = hub.history();
    assert(hist.size() == 4);
    assert(hist.front().frameMs == 2.0f && hist.back().frameMs == 5.0f);
    hub.clear();
    assert(hub.empty() && hub.size() == 0);
  }

  // Zero-codec peer and paint-hook peer, as before.
  {
    Block probe = toBlock(Probe{});
    Box pbox = place(probe, {0, 720, 0, 480});
    OpsRecorder prec;
    host::LayerFeed stubFeed;
    Engine pe;
    paintTree(probe, pbox, pe, prec, stubFeed);
    assert(prec.ops.size() == 1 && prec.ops[0].kind == Op::Kind::Entry);
    assert(prec.ops[0].entry.kind == "Probe");
    std::vector<std::uint8_t> pbytes;
    host::encodeOps(prec.ops, pbytes);
    std::vector<Op> pback;
    assert(host::decodeOps(pbytes.data(), pbytes.size(), pback));
    assert(pback == prec.ops);
  }
  {
    Block cp = toBlock(CustomPainted{});
    Box cbox = place(cp, {0, 720, 0, 480});
    glim::paint::Context pctx;
    pctx.setSize({720, 480});
    pctx.beginFrame();
    {
      host::LayerFeed pf;
      host::ContextSink ps{pctx};
      Engine pe;
      paintTree(cp, cbox, pe, ps, pf);
    }
    pctx.finish();
    int strokes = 0, fills = 0;
    glim::paint::visitGroup(
        pctx.scene().root,
        [&](const glim::paint::Shape& s) {
          if (std::holds_alternative<glim::paint::Stroke>(s))
            ++strokes;
          else if (std::holds_alternative<glim::paint::FillRounded>(s))
            ++fills;
        },
        [&](const glim::paint::Group&) {}
    );
    assert(strokes == 1 && fills == 0);
    OpsRecorder cr;
    {
      host::LayerFeed pf;
      Engine pe;
      paintTree(cp, cbox, pe, cr, pf);
    }
    assert(cr.ops.size() == 1 && cr.ops[0].kind == Op::Kind::Stroke);
    assert(cr.ops[0].width == 1.0f);
  }

  std::cout << "smarthome_test ok\n";
  return EXIT_SUCCESS;
}
