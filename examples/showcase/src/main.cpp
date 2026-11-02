#include <iostream>
#include <sstream>
#include <string>

#include "Showcase.h"
#include "glim/toolkit/Toolkit.h"
#include "glim/toolkit/host/Host.h"
#include "glim/toolkit/host/Paint.h"
#include "widgets/Button.h"
#include "widgets/Checkbox.h"
#include "widgets/Field.h"
#include "widgets/Progress.h"
#include "widgets/Segmented.h"
#include "widgets/Slider.h"
#include "widgets/Switch.h"

using namespace glim::toolkit;

namespace glim {
namespace toolkit {

Block showcaseTree(Scope app) {
  auto wifi = app.state("wifi", true);
  auto sound = app.state("sound", false);
  auto range = app.state("range", 1);
  auto level = app.state("level", 0.4f);
  auto taps = app.state("taps", 0);
  auto section = [&](const char* label, Block content) -> Block {
    return column(gap(6), lines(label), std::move(content));
  };
  std::string count = "Pressed " + std::to_string(*taps) + " times";
  return roll(
      vertical(),
      column(
          gap(20),
          pad(16),
          section(
              "Button",
              column(
                  gap(8),
                  row(gap(8),
                      button(
                          "Export",
                          [t = taps] { t.update([](int& v) { ++v; }); },
                          primary()
                      ),
                      button("Cancel", [t = taps] { t = 0; })),
                  lines(count)
              )
          ),
          section(
              "Text field",
              fieldRow("Name", "First and last name", kShowcaseSeatId)
          ),
          section("Switch", toggle(wifi)),
          section("Checkbox", checkbox("Sound", sound)),
          section(
              "Segmented", segmented({"Day", "Week", "Month"}, range)
          ),
          section("Slider", slider(level)),
          section("Progress", progress(level))
      )
  );
}

}  // namespace toolkit
}  // namespace glim

namespace {

glim::toolkit::Engine engineFor(glim::toolkit::Store& store) {
  glim::toolkit::Engine e = glim::toolkit::host::measureEngine();
  e.store = &store;
  return e;
}

}  // namespace

std::string dumpTree(Block tree, Constraints c) {
  std::ostringstream os;
  Engine e = host::measureEngine();
  Box box = e.place(tree, c);
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree, box, e);
  return os.str();
}

std::string showcaseDump(Store& store) {
  return dumpTree(
      showcaseTree(rootScope(engineFor(store))), {0, 720, 0, 480}
  );
}

std::string showcaseFocusDump(Store& store) {
  Engine e = engineFor(store);
  Block tree = showcaseTree(rootScope(e));
  Constraints c{0, 720, 0, 480};
  Box box = e.place(tree, c);
  std::ostringstream os;
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree, box, e);
  Box rb = focusRingBox(tree, box, 0, 2.0f, e);
  os << kFocusRingKind << " [" << rb.x << "," << rb.y << " " << rb.w
     << "x" << rb.h << "]\n";
  return os.str();
}

void paintShowcaseScene(
    glim::paint::Context& ctx,
    host::LayerFeed& feed,
    Store& store,
    int focus = -1,
    bool focusVisible = true
) {
  Engine engine = engineFor(store);
  Block screen = showcaseTree(rootScope(engine));
  Box box = engine.place(screen, {0, ctx.size().x, 0, ctx.size().y});
  host::paintScreen(
      ctx, screen, box, engine, feed, focus, focusVisible
  );
}

#ifndef GLIM_GOLDEN
#if defined(__ANDROID__)
extern "C" int glim_app_main() {
#else
int main() {
#endif
  // The whole app: one tree function. The host owns Store, Screen,
  // measuring Engine, and paint — scoped state allocates into the
  // host Store where it renders.
  return host::run({"Glim Showcase", 720, 480}, [](const Engine& e) {
    return showcaseTree(rootScope(e));
  });
}
#endif
