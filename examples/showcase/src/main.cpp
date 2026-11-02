#include <iostream>
#include <sstream>
#include <string>

#include "Showcase.h"
#include "elements/Button.h"
#include "elements/Card.h"
#include "elements/Checkbox.h"
#include "elements/Display.h"
#include "elements/Field.h"
#include "elements/Icons.h"
#include "elements/Progress.h"
#include "elements/Segmented.h"
#include "elements/Slider.h"
#include "elements/Switch.h"
#include "glim/toolkit/Toolkit.h"
#include "paint/Bridge.h"
#include "paint/Keys.h"
#include "paint/MeasureBridge.h"
#include "paint/Ops.h"
#include "paint/Overlay.h"
#include "paint/VideoFeed.h"
#ifndef GLIM_GOLDEN
#include "run.h"
#endif
#ifndef GLIM_GOLDEN
#include <unordered_map>

#include "paint/Seats.h"
#endif

using namespace glim::toolkit;
using namespace glim::toolkit::paint;

// Light design bed: a scrollable column of labeled rows — display
// echo, buttons, video plate, wrapped paragraph, frosted card, eager
// list. One screen, everything touchable, nothing else.
namespace {

const char* kMonths[12] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

// Dashboard month window: base Jun→Sep 2025 shifted by whole months.
std::string monthLabel(int total) {
  int y = total / 12, m = total % 12;
  if (m < 0) {
    m += 12;
    --y;
  }
  return std::string(kMonths[static_cast<std::size_t>(m)]) + " " +
         std::to_string(y);
}

}  // namespace

Block showcaseTree(Showcase s) {
  auto section = [&](const char* label, Block content) -> Block {
    return column(gap(6), lines(label), std::move(content));
  };
  auto iconBtn = [&](Block glyph, Action onTap) -> Block {
    return card(
        pad(0),
        radius(12),
        tap(std::move(glyph), std::move(onTap), pad(8))
    );
  };
  // Reference stats row: icon left, gray caption over white value.
  auto stat = [&](Block glyph, const char* label, const char* value
              ) -> Block {
    return row(
        center(),
        gap(12),
        std::move(glyph),
        column(gap(2), dimLines(label), display(value))
    );
  };
  int base = 2025 * 12 + 5;  // Jun 2025, 0-based month
  std::string startL = monthLabel(base + *s.span);
  std::string endL = monthLabel(base + 3 + *s.span);
  Block toolbarRow = row(
      center(),
      gap(12),
      iconBtn(icon<IconGear>(), [n = s.notice] { n = "Settings"; }),
      divider(),
      button(
          "v Export CSV",
          [n = s.notice] { n = "Exported 10,367 rows"; },
          primary()
      ),
      divider(),
      segmented({"Split", "Board", "List"}, s.viewSel),
      divider(),
      iconBtn(icon<IconExpand>(), [n = s.notice] { n = "Fullscreen"; })
  );
  Block toolbar =
      s.notice->empty()
          ? std::move(toolbarRow)
          : column(gap(4), std::move(toolbarRow), lines(*s.notice));
  return roll(
      vertical(),
      column(
          gap(20),
          pad(16),
          // Hero combos mirror the reference: no section labels, just
          // the pills. The input catalog below keeps labels.
          row(gap(8),
              card(
                  radius(18),
                  row(center(),
                      gap(12),
                      tap(
                          lines("<"),
                          [sp = s.span] { sp = *sp - 1; },
                          pad(14, 7)
                      ),
                      divider(),
                      lines(startL),
                      lines("-"),
                      lines(endL),
                      divider(),
                      tap(
                          lines(">"),
                          [sp = s.span] { sp = *sp + 1; },
                          pad(14, 7)
                      ))
              ),
              iconBtn(
                  icon<IconDownload>(),
                  [n = s.notice] { n = "Download started"; }
              )),
          std::move(toolbar),
          row(center(),
              gap(12),
              segmented({"Moon", "Sun"}, s.mode),
              iconBtn(
                  icon<IconBell>(),
                  [n = s.notice] { n = "3 new alerts"; }
              ),
              avatar("AR"),
              segmented(
                  {"Overview", "User Insights", "Behavior"}, s.tabSel
              )),
          card(
              radius(18),
              row(center(),
                  gap(16),
                  stat(icon<IconTicket>(), "ORDERS", "10,367"),
                  divider(),
                  stat(icon<IconPerson>(), "VISITORS", "12,567"),
                  divider(),
                  stat(icon<IconPie>(), "AVR REVENUE", "$12,875"))
          ),
          // Seat reconcile: the rect is a toolkit value like any
          // other; the bridge turns it into a structural slot op and
          // syncSeats hosts the native field in the hole. The row
          // carries its own label + hint; committed-text echo waits
          // for the native-to-toolkit sync slice.
          section(
              "Text field",
              fieldRow("Name", "First and last name", kShowcaseSeatId)
          ),
          section("Switch", toggle(s.wifi)),
          section("Checkbox", checkbox("Sound", s.sound)),
          section(
              "Segmented", segmented({"Day", "Week", "Month"}, s.range)
          ),
          section("Slider", slider(s.level)),
          section("Progress", progress(s.level))
      )
  );
}

std::string dumpTree(Block tree, Constraints c) {
  std::ostringstream os;
  Engine e = measureEngine();
  Box box = e.place(tree, c);
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree, box, e);
  return os.str();
}

std::string showcaseDump(Showcase s) {
  return dumpTree(showcaseTree(s), {0, 720, 0, 480});
}

std::string showcaseFocusDump(Showcase s) {
  Block tree = showcaseTree(s);
  Constraints c{0, 720, 0, 480};
  Engine e = measureEngine();
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
    VideoFeed& feed,
    Showcase s,
    int focus = -1,
    bool focusVisible = true
) {
  Block screen = showcaseTree(s);
  Box box =
      measureEngine().place(screen, {0, ctx.size().x, 0, ctx.size().y});
  // Window base is harness chrome, not tree content.
  ctx.setFillColor(Tokens::kWindowBg);
  ctx.fill(glim::Rect{{0, 0}, ctx.size()});
  ContextSink sink{ctx};
  paintShowcaseInto(
      screen, box, sink, feed, focus, measureEngine(), focusVisible
  );
}

#ifndef GLIM_GOLDEN
namespace {

// Retained across frames: handles own the cells, focus persists while
// trees rebuild every frame.
struct ShowcaseRuntime {
  Store store;
  State<std::string> out;
  State<bool> wifi;
  State<bool> sound;
  State<int> range;
  State<float> level;
  State<int> span;
  State<int> mode;
  State<int> viewSel;
  State<int> tabSel;
  State<std::string> notice;
  int focus = -1;
  // Focus-visible modality: pointer sets focus silently, keyboard
  // shows the ring. Modern desktop convention; see Bridge.h.
  bool focusVisible = false;
  // Pointer capture: PointerDown stores the hit focusable; pressed
  // moves route straight to it (no re-hit-test) so a drag never
  // drops out when the finger leaves the box, plus the pointer id so
  // a second touch can't hijack the drag.
  int drag = -1;
  int dragId = 0;
  ShowcaseRuntime()
      : out(store.make<std::string>("12+3")),
        wifi(store.make<bool>(true)),
        sound(store.make<bool>(false)),
        range(store.make<int>(1)),
        level(store.make<float>(0.4f)),
        span(store.make<int>(0)),
        mode(store.make<int>(0)),
        viewSel(store.make<int>(0)),
        tabSel(store.make<int>(0)),
        notice(store.make<std::string>("")) {}
};

ShowcaseRuntime& runtime() {
  static ShowcaseRuntime r;
  return r;
}

bool showcaseInteract(
    const glim::shell::Event& e, ExampleFrame& frame
) {
  using T = glim::shell::EventType;
  auto& rt = runtime();
  Block screen = showcaseTree(
      {rt.out,
       rt.wifi,
       rt.sound,
       rt.range,
       rt.level,
       rt.span,
       rt.mode,
       rt.viewSel,
       rt.tabSel,
       rt.notice}
  );
  Constraints c{0, frame.size.x, 0, frame.size.y};
  Box box = measureEngine().place(screen, c);
  if (e.type() == T::PointerDown) {
    float x = e.x(), y = e.y();
    Event tev = Event::borrow(PointerDown{x, y});
    Engine me = measureEngine();
    if (dispatchPointer(screen, box, x, y, tev, me)) {
      // Capture only when the point truly lands in a focus box;
      // otherwise keep the previous focus and capture nothing.
      int hit = focusAt(screen, box, x, y, me);
      if (hit >= 0) {
        rt.focus = hit;
        rt.drag = hit;
        rt.dragId = e.pointerId();
      }
      rt.focusVisible = false;
      return true;
    }
    rt.drag = -1;
    return false;
  }
  if (e.type() == T::PointerUp) {
    float x = e.x(), y = e.y();
    Event tev = Event::borrow(PointerUp{x, y});
    rt.drag = -1;
    return dispatchPointer(screen, box, x, y, tev, measureEngine());
  }
  if (e.type() == T::PointerMove) {
    // Pressed drags scrub (slider); hover moves never dispatch —
    // mapped here from the shell button, not stored in the tree.
    // Captured drags route to the PointerDown target directly and
    // defer paint to the next Frame tick: a synchronous Metal present
    // per drag event blocks on vsync and queues input behind it.
    bool pressed = e.button() != glim::shell::PointerButton::None;
    if (!pressed) return false;
    float x = e.x(), y = e.y();
    PointerMove mv{x, y, pressed};
    Event tev = Event::borrow(mv);
    Engine me = measureEngine();
    if (rt.drag >= 0) {
      // A second finger while dragging is ignored, never hijacked.
      if (e.pointerId() != rt.dragId) return false;
      // Core capture: routed (not consumed) keeps the drag; stale
      // falls back to a fresh hit-test.
      if (dispatchCaptured(screen, box, rt.drag, tev, me)) return false;
      rt.drag = -1;
    }
    return dispatchPointer(screen, box, x, y, tev, me);
  }
  if (e.type() == T::KeyDown) {
    auto key = showcaseKey(e.key());
    if (!key) return false;
    rt.focus = deliverKey(screen, box, rt.focus, *key, measureEngine());
    rt.focusVisible = true;
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
  static std::unordered_map<std::uint32_t, glim::Rect> attached;
  paintShowcaseScene(
      frame.context,
      feed,
      {rt.out,
       rt.wifi,
       rt.sound,
       rt.range,
       rt.level,
       rt.span,
       rt.mode,
       rt.viewSel,
       rt.tabSel,
       rt.notice},
      rt.focus,
      rt.focusVisible
  );
  syncSeats(frame.window, frame.context.scene(), attached);
}
#endif
