#pragma once

// Host loop: shell + paint + toolkit in one place; apps own only states, tree, paint.
#include <glim/math.h>
#include <glim/paint/Context.h>
#include <glim/shell/Event.h>
#include <glim/shell/Window.h>
#if !GLIM_SOFTWARE
#include <glim/gpu/Device.h>
#endif

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "glim/toolkit/A11y.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Screen.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/host/SeatView.h"
#include "glim/toolkit/host/Telemetry.h"

namespace glim {
namespace toolkit {
namespace host {

struct Config {
  const char* title = "Glim";
  int width = 720;
  int height = 480;
  // macOS modern chrome: hide the system title, extend content under
  // the titlebar, keep traffic lights. The app paints its own bar
  // and empty header areas drag the window (see Host drag strip).
  bool customTitlebar = false;
  // Draggable strip height in logical px from the top edge.
  float titleDragHeight = 52.0f;
  // Profiling: replay this input script into the live window, then
  // close it and print a latency report (see Drive.h). The trace path,
  // when set, also gets a Chrome trace of events and paints.
  std::string drive;
  std::string driveTrace;
  // Profiling: present the first frame, print the startup phases (see
  // Startup.h) and exit.
  bool startup = false;
  // Per-frame context setup before the default paint (e.g. registering
  // cached images); same hook as TestApp::prepare. Ignored with a Record.
  std::function<void(paint::Context&, Store&)> prepare;
};

struct Frame {
  paint::Context& context;
  shell::Window& window;
  Vec2 size;
  Rect safeArea;
  bool reduceTransparency = false;
  float time = 0.f;
#if !GLIM_SOFTWARE
  gpu::Device& device;
#endif
};

// Shell-to-toolkit key mapping. Unmapped keys yield nullopt.
std::optional<Key> mapKey(shell::Key key);

using Build = std::function<Block(const Engine&)>;
using Record = std::function<void(Frame&, const Block&, const Box&)>;
// A11y publish seam: shells bridge the node map to OS a11y APIs. Null sink
// (default) skips the walk entirely — zero cost when unused.
using A11ySink = std::function<void(const std::vector<A11yNode>& nodes, int focused)>;

// A null record paints the screen with the default focus ring and layers.
// Getting-started path: the host owns Store, Screen, measuring Engine, and
// the default paint; scoped state allocates into the host Store where it renders.
// Telemetry is opt-in both ways: hub retains the ring for pull (dashboard
// labels/charts), sink fires per painted frame for push (log/ship elsewhere).
int run(
    const Config& config,
    Screen& screen,
    Store& store,
    const Engine& engine,
    Build build,
    Record record,
    SeatViewFactory makeSeatView = nullptr,
    A11ySink a11ySink = nullptr,
    TelemetryHub* telemetry = nullptr,
    TelemetrySink telemetrySink = nullptr
);

// Getting-started path: the host owns Store, Screen, measuring Engine, and
// the default paint; scoped state allocates into the host Store where it renders.
int run(
    const Config& config,
    Build build,
    Record record = Record{},
    SeatViewFactory makeSeatView = nullptr,
    A11ySink a11ySink = nullptr,
    TelemetryHub* telemetry = nullptr,
    TelemetrySink telemetrySink = nullptr
);

}  // namespace host
}  // namespace toolkit
}  // namespace glim
