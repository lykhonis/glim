#include <iostream>
#include <sstream>
#include <string>

#include "Showcase.h"
#include "glim/toolkit.h"
#include "glim/toolkit/host/Host.h"
#include "glim/toolkit/host/Paint.h"
#include "widgets/Button.h"
#include "widgets/Card.h"
#include "widgets/Checkbox.h"
#include "widgets/Field.h"
#include "widgets/Glass.h"
#include "widgets/Progress.h"
#include "widgets/Segmented.h"
#include "widgets/Slider.h"
#include "widgets/Switch.h"

using namespace glim::toolkit;

namespace glim {
namespace toolkit {

Block showcaseTree(Scope app) {
  auto range = app.state("range", 1);
  Anim<float> rangePos{app, "range-pos", 1.0f};
  auto level = app.state("level", 0.4f);
  auto taps = app.state("taps", 0);
  auto vscroll = app.state("vscroll", 0.0f);
  auto hscroll = app.state("hscroll", 0.0f);
  auto section = [&](const char* label, Block content) -> Block {
    return column(gap(6), lines(label), std::move(content));
  };
  std::string count = "Pressed " + std::to_string(*taps) + " times";
  return roll(
      vertical(),
      scrollOffset(vscroll),
      column(
          gap(20),
          pad(16),
          section(
              "Button",
              row(gap(24),
                  center(),
                  veilOrb(glass(36)),
                  veilCard(
                      glass(16),
                      gap(8),
                      row(gap(8),
                          button(
                              {.label = "Export",
                               .onTap =
                                   [t = taps] {
                                     t.update([](int& v) { ++v; });
                                   }},
                              primary()
                          ),
                          button(
                              {.label = "Cancel",
                               .onTap = [t = taps] { t = 0; }}
                          )),
                      lines(count)
                  ))
          ),
          section(
              "Text field",
              fieldRow(
                  {.label = "Name",
                   .hint = "First and last name",
                   .id = kShowcaseSeatId}
              )
          ),
          section(
              "Multiline text",
              column(
                  gap(8),
                  lines(
                      "Glim lays out text in logical pixels and wraps "
                      "greedy word by word. Long words split at char "
                      "boundaries when they cannot fit on one line.",
                      wrap()
                  ),
                  row(lines(
                      "Centered caption",
                      wrap(),
                      alignCenter(),
                      expand()
                  )),
                  row(lines(
                      "Right-aligned caption",
                      wrap(),
                      alignRight(),
                      expand()
                  ))
              )
          ),
          section("Switch", toggle({.value = {"value", true}})),
          section("Checkbox", checkbox({.label = "Sound"})),
          section(
              "Overflow & horizontal scroll",
              column(
                  gap(8),
                  lines(
                      "The row below is wider than the viewport. "
                      "Wheel over it to pan sideways.",
                      wrap()
                  ),
                  [&] {
                    Roll strip;
                    strip.axis = kAxisHorizontal;
                    strip.gap = 8;
                    strip.scrollOff = hscroll;
                    for (int i = 0; i < 12; ++i) {
                      strip.children.push_back(toBlock(
                          lines("chip-" + std::to_string(i) + "-pill")
                      ));
                    }
                    return toBlock(std::move(strip));
                  }()
              )
          ),
          section(
              "Segmented",
              segmented(
                  {.options = {"Day", "Week", "Month"},
                   .selected = range,
                   .selPos = rangePos}
              )
          ),
          section("Slider", slider({.value = level})),
          section("Progress", progress({.value = level}))
      )
  );
}

}  // namespace toolkit
}  // namespace glim

namespace {

glim::toolkit::Engine engineFor(glim::toolkit::Store& store) {
  glim::toolkit::Engine e = glim::toolkit::host::measureEngine();
  e.store = store;
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

std::string showcaseA11yDump(Store& store) {
  Engine e = engineFor(store);
  Block tree = showcaseTree(rootScope(e));
  Box box = e.place(tree, {0, 720, 0, 480});
  std::ostringstream os;
  dumpA11y(os, a11yNodes(tree, box, e));
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
  // One tree function; the host owns Store, Screen, Engine, and paint.
  return host::run({"Glim Showcase", 720, 480}, [](const Engine& e) {
    return showcaseTree(rootScope(e));
  });
}
#endif
