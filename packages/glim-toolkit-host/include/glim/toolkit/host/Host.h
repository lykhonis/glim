#pragma once

// Host loop for toolkit apps: shell + paint + toolkit in one place so
// apps own only states, tree, and paint.
#include <glim/math.h>
#include <glim/paint/Context.h>
#include <glim/shell/Event.h>
#include <glim/shell/Window.h>
#if !GLIM_SOFTWARE
#include <glim/gpu/Device.h>
#endif

#include <functional>
#include <optional>
#include <vector>

#include "glim/toolkit/A11y.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Screen.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/host/SeatView.h"

namespace glim {
namespace toolkit {
namespace host {

struct Config {
  const char* title = "Glim";
  int width = 720;
  int height = 480;
#if defined(NDEBUG)
  bool overlay = false;
#else
  bool overlay = true;
#endif
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
// A11y publish seam (T6): shells bridge the node map to OS a11y APIs
// later (NSAccessibility, UIAccessibility, ...). Null sink (default)
// skips the walk entirely — zero cost when unused.
using A11ySink = std::function<void(const std::vector<A11yNode>& nodes, int focused)>;

// Null factory means the platform default. Embedders pass their own to
// create views inside an existing hierarchy.
int run(
    const Config& config,
    Screen& screen,
    Store& store,
    const Engine& engine,
    Build build,
    Record record,
    SeatViewFactory makeSeatView = nullptr,
    A11ySink a11ySink = nullptr
);

// Getting-started path: the host owns Store, Screen, measuring Engine,
// and the default paint (window fill, tree, focus ring). The app passes
// only its tree — scoped state allocates into the host Store where it
// renders. Embedders and tests use the overload above to inject.
int run(
    const Config& config,
    Build build,
    Record record = Record{},
    SeatViewFactory makeSeatView = nullptr,
    A11ySink a11ySink = nullptr
);

}  // namespace host
}  // namespace toolkit
}  // namespace glim
