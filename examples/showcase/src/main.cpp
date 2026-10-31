#include <iostream>
#include <sstream>
#include <string>

#include "Showcase.h"
#include "elements/Button.h"
#include "elements/Card.h"
#include "elements/Video.h"
#include "glim/toolkit/Toolkit.h"
#include "paint/Bridge.h"
#include "paint/Keys.h"
#include "paint/Ops.h"
#include "paint/Overlay.h"
#include "paint/VideoFeed.h"
#ifndef GLIM_GOLDEN
#include "run.h"
#endif

using namespace glim::toolkit;
using namespace glim::toolkit::paint;

// Light design bed: a scrollable column of labeled rows — display
// echo, buttons, video plate, wrapped paragraph, frosted card, eager
// list. One screen, everything touchable, nothing else.
Block showcaseTree(Showcase s) {
  auto o = s.out;
  Block display =
      roll(horizontal(), scrollOffset(State<float>{}), lines(*o));
  auto digit = [&](const char* label, char d) -> Block {
    return button(label, [o, d] { o = *o + d; }, expand());
  };
  auto section = [&](const char* label, Block content) -> Block {
    return column(gap(6), lines(label), std::move(content));
  };
  return roll(
      vertical(),
      column(
          gap(20),
          section("Display", std::move(display)),
          section(
              "Buttons",
              row(gap(8),
                  digit("7", '7'),
                  digit("8", '8'),
                  button(
                      "=", [o] { o = "="; }, primary(), expand()
                  ))
          ),
          section("Video", video()),
          section(
              "Multiline",
              lines(
                  "Frosted card over foreign video, all from one tree. "
                  "Wrap keeps words whole, splits long words, and the "
                  "list below spills past its viewport by design.",
                  wrap()
              )
          ),
          section(
              "Card",
              card(
                  glassClear(),
                  radius(6),
                  gap(8),
                  lines("Frosted card"),
                  lines("backdrop blur plus tint")
              )
          ),
          section(
              "List",
              list(
                  4,
                  [](int i) {
                    return lines("item" + std::to_string(i));
                  }
              )
          )
      )
  );
}

std::string dumpTree(Block tree, Constraints c) {
  std::ostringstream os;
  Box box = place(tree, c);
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree, box);
  return os.str();
}

std::string showcaseDump(Showcase s) {
  return dumpTree(showcaseTree(s), {0, 720, 0, 480});
}

std::string showcaseFocusDump(Showcase s) {
  Block tree = showcaseTree(s);
  Constraints c{0, 720, 0, 480};
  Box box = place(tree, c);
  std::ostringstream os;
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree, box);
  Box rb = focusRingBox(tree, box, 0);
  os << kFocusRingKind << " [" << rb.x << "," << rb.y << " " << rb.w
     << "x" << rb.h << "]\n";
  return os.str();
}

void paintShowcaseScene(
    glim::paint::Context& ctx,
    VideoFeed& feed,
    Showcase s,
    int focus = 0
) {
  Block screen = showcaseTree(s);
  Box box = place(screen, {0, ctx.size().x, 0, ctx.size().y});
  // Window base is harness chrome, not tree content: the video plate
  // and frosted card ride above it.
  ctx.setFillColor(Tokens::kWindowBg);
  ctx.fill(glim::Rect{{0, 0}, ctx.size()});
  ContextSink sink{ctx};
  paintShowcaseInto(screen, box, sink, feed, focus);
}

#ifndef GLIM_GOLDEN
namespace {

// Retained across frames: handles own the cells, focus persists while
// trees rebuild every frame.
struct ShowcaseRuntime {
  Store store;
  State<std::string> out;
  int focus = 0;
  ShowcaseRuntime() : out(store.make<std::string>("12+3")) {}
};

ShowcaseRuntime& runtime() {
  static ShowcaseRuntime r;
  return r;
}

int focusAt(
    const Block& tree, const Box& box, float x, float y, int fallback
) {
  auto items = focusables(tree, box);
  for (std::size_t i = 0; i < items.size(); ++i) {
    Box b = focusBox(tree, box, static_cast<int>(i));
    if (b.x <= x && x < b.x + b.w && b.y <= y && y < b.y + b.h)
      return static_cast<int>(i);
  }
  return fallback;
}

bool showcaseInteract(
    const glim::shell::Event& e, ExampleFrame& frame
) {
  using T = glim::shell::EventType;
  auto& rt = runtime();
  Block screen = showcaseTree({rt.out});
  Constraints c{0, frame.size.x, 0, frame.size.y};
  Box box = place(screen, c);
  if (e.type() == T::PointerDown) {
    float x = e.x(), y = e.y();
    Event tev = Event::borrow(PointerDown{x, y});
    if (dispatchPointer(screen, box, x, y, tev)) {
      rt.focus = focusAt(screen, box, x, y, rt.focus);
      return true;
    }
    return false;
  }
  if (e.type() == T::KeyDown) {
    auto key = showcaseKey(e.key());
    if (!key) return false;
    rt.focus = deliverKey(screen, box, rt.focus, *key);
    return true;
  }
  return false;
}

}  // namespace

bool onExampleEvent(const glim::shell::Event& e, ExampleFrame& frame) {
  return showcaseInteract(e, frame);
}

void recordExample(ExampleFrame& frame) {
  auto& rt = runtime();
  static VideoFeed feed;
  paintShowcaseScene(frame.context, feed, {rt.out}, rt.focus);
}
#endif
