// Glim Home: a one-page smart-home dashboard that shows the toolkit's
// pieces working together — glass cards over a glowing backdrop,
// switches and sliders with motion, a reflowing grid, scrolling under
// a glass header, a light/dark theme swap, and sensors that are really
// the host's own frame telemetry.

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Overview.h"
#include "SmartHome.h"
#include "VillaLoad.h"
#include "glim/toolkit.h"
#include "glim/toolkit/host/Host.h"
#include "glim/toolkit/host/Paint.h"
#include "glim/toolkit/host/Probe.h"
#include "glim/toolkit/host/Telemetry.h"
#include "widgets/Backdrop.h"
#include "widgets/Glide.h"
#include "widgets/HomeTheme.h"
#include "widgets/Strut.h"
#include "widgets/Switch.h"
#include "widgets/Type.h"

using namespace glim::toolkit;

namespace glim {
namespace toolkit {

namespace {

// Floating glass header: matches Host titleDragHeight so the strip
// doubles as the window drag zone.
constexpr float kHeaderH = 52.0f;
// Left inset clears the macOS traffic lights under the custom titlebar.
constexpr int kHeaderInsetL = 84;

std::string cellKey(
    const home::Room& room, const home::Device& d, const char* field
) {
  return "home/" + room.id + "/" + d.id + "/" + field;
}

Block header(State<bool> live, State<bool> light, std::uint32_t tint) {
  return wide(
      row(gap(12),
          center(),
          pad(24, 12),
          GlassTag{false, 0, GlassEdge::None, tint},
          strut({.width = static_cast<float>(kHeaderInsetL - 24)}),
          titleLines("Glim Home"),
          lines(home::feedStatus(*live)),
          column(expand(), gap(0)),
          lines("Live updates"),
          toggle({.value = live}, semantic("Live updates")),
          strut({.width = 8.0f}),
          lines("Light mode"),
          toggle({.value = light}, semantic("Light mode")))
  );
}

const home::Device& byId(
    const std::vector<home::Device>& devices, const char* id
) {
  return *std::find_if(
      devices.begin(),
      devices.end(),
      [&](const home::Device& d) { return d.id == id; }
  );
}

}  // namespace

DeviceCells deviceCells(
    Scope app, const home::Room& room, const home::Device& seed
) {
  return {
      app.state(cellKey(room, seed, "on"), seed.on),
      app.state(cellKey(room, seed, "level"), seed.level)
  };
}

std::vector<home::Device> liveDevices(
    Scope app, const home::Room& room
) {
  std::vector<home::Device> out = room.devices;
  for (home::Device& d : out) {
    DeviceCells c = deviceCells(app, room, d);
    d.on = *c.on;
    d.level = *c.level;
  }
  return out;
}

home::FrameSample frameSample(const host::FrameTelemetry& t) {
  return {t.time, t.frameMs, t.buildMs + t.recordMs + t.encodeMs};
}

Block smartHomeTree(Scope app, const home::SensorLog& log) {
  auto light = app.state("theme", true);
  auto live = app.state("live", true);
  auto vscroll = app.state("vscroll", 0.0f);
  const std::uint32_t tint =
      *light ? ShowLight::kGlassTint : Show::kGlassTint;

  // The overview drives the living room's thermostat and ceiling light.
  const home::Room& living = home::rooms().front();
  const std::vector<home::Device> devices = liveDevices(app, living);
  const home::Device& thermostat = byId(devices, "climate");
  const home::Device& ceiling = byId(devices, "ceiling");
  const home::Readings readings = home::readings(log);
  const Shown shown{
      glide(app, "glide/energy", readings.energy),
      glide(app, "glide/indoor", readings.indoorC),
      coast(app, "glide/thermostat", thermostat.level),
      glide(app, "glide/light", ceiling.level),
      glide(app, "glide/brightness", home::brightness({ceiling})),
      slide(
          app,
          "glide/solar",
          readings.curveHead,
          home::kBucketSec,
          AreaChart::kLead
      ),
  };
  OverviewData data{
      readings,
      shown,
      thermostat,
      deviceCells(app, living, thermostat),
      ceiling,
      deviceCells(app, living, ceiling),
      app.state("wifi", true),
      tint,
  };

  // The roll fills the window and the header floats over its top; the
  // strut keeps the first row clear of the header at rest.
  return smartHomeThemed(
      {.light = *light,
       .child = stack(
           expand(),
           backdrop(),
           roll(
               vertical(),
               scrollOffset(vscroll),
               column(
                   gap(0),
                   pad(36),
                   strut({.height = kHeaderH}),
                   overviewPage(data)
               )
           ),
           header(live, light, tint)
       )}
  );
}

}  // namespace toolkit
}  // namespace glim

namespace {

Engine engineFor(Store& store) {
  Engine e = host::measureEngine();
  e.store = store;
  return e;
}

std::string dumpTree(Block tree, Constraints c) {
  std::ostringstream os;
  Engine e = host::measureEngine();
  Box box = e.place(tree, c);
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree, box, e);
  return os.str();
}

}  // namespace

std::string smartHomeDump(Store& store) {
  return dumpTree(
      smartHomeTree(rootScope(engineFor(store))), {0, 1024, 0, 640}
  );
}

std::string smartHomeFocusDump(Store& store) {
  Engine e = engineFor(store);
  Block tree = smartHomeTree(rootScope(e));
  Box box = e.place(tree, {0, 1024, 0, 640});
  std::ostringstream os;
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree, box, e);
  Box rb = focusRingBox(tree, box, 0, 2.0f, e);
  os << kFocusRingKind << " [" << rb.x << "," << rb.y << " " << rb.w
     << "x" << rb.h << "]\n";
  return os.str();
}

std::string smartHomeA11yDump(Store& store) {
  Engine e = engineFor(store);
  Block tree = smartHomeTree(rootScope(e));
  Box box = e.place(tree, {0, 1024, 0, 640});
  std::ostringstream os;
  dumpA11y(os, a11yNodes(tree, box, e));
  return os.str();
}

void paintSmartHomeScene(
    glim::paint::Context& ctx,
    host::LayerFeed& feed,
    Store& store,
    int focus = -1,
    bool focusVisible = true
) {
  Engine engine = engineFor(store);
  ensureVilla(ctx, store);
  Block screen = smartHomeTree(rootScope(engine));
  Box box = engine.place(screen, {0, ctx.size().x, 0, ctx.size().y});
  host::paintScreen(
      ctx, screen, box, engine, feed, focus, focusVisible
  );
}

#ifndef GLIM_GOLDEN
#if defined(__ANDROID__)
extern "C" int glim_app_main() {
  int argc = 0;
  char** argv = nullptr;
#else
int main(int argc, char** argv) {
#endif
  host::E2eFlags flags = host::parseE2e(argc, argv);
  if (flags.help) {
    std::cout << "usage: glim-smarthome " << host::kE2eUsage << "\n";
    return 0;
  }
  // Headless driver: the same tree, no window.
  {
    if (flags.any()) {
      host::TestApp t;
      t.w = 1240;
      t.h = 760;
      t.build = [](const Engine& e) {
        return smartHomeTree(rootScope(e));
      };
      t.prepare = ensureVilla;
      return host::runE2e(t, flags);
    }
  }
  // The sink folds every painted frame into the sensor log the
  // overview reads. Live keeps frames coming with a never-settling
  // frame watch; off, frames (and readings) only follow input.
  static glim::home::SensorLog log;
  static Store::Watch watch;
  Store store;
  Screen screen;
  Engine engine = host::measureEngine();
  return host::run(
      {.title = "Glim Home",
       .width = 1240,
       .height = 760,
       .customTitlebar = true,
       .drive = flags.drive,
       .driveTrace = flags.driveTrace,
       .startup = flags.startup,
       .prepare = ensureVilla},
      screen,
      store,
      engine,
      [](const Engine& e) {
        Scope app = rootScope(e);
        if (!*app.state("live", true)) {
          watch.reset();
        } else if (!watch.live()) {
          State<float> tick = app.state("live-tick", 0.0f);
          watch = e.store->get().watchFrame([tick](float now) mutable {
            tick = now;
            return true;
          });
        }
        return smartHomeTree(
            app, log.closed.empty() ? glim::home::demoLog() : log
        );
      },
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      [](const host::FrameTelemetry& t) {
        log = glim::home::logSample(std::move(log), frameSample(t));
      }
  );
}
#endif
