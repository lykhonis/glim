#include "Showcase.h"

#include <glim/image/Image.h>
#include <glim/image/Png.h>
#include <glim/paint/Context.h>
#include <glim/paint/Scene.h>
#include <glim/paint/Software.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "glim/toolkit.h"
#include "glim/toolkit/host/Codec.h"
#include "glim/toolkit/host/Host.h"
#include "glim/toolkit/host/Paint.h"
#include "widgets/AlertCard.h"
#include "widgets/Checkbox.h"
#include "widgets/Mover.h"
#include "widgets/Segmented.h"
#include "widgets/ShowcaseTheme.h"
#include "widgets/StageRow.h"
#include "widgets/StripChart.h"
#include "widgets/Switch.h"

using namespace glim::toolkit;

std::string showcaseDump(Store& store);
std::string showcaseFocusDump(Store& store);
std::string showcaseA11yDump(Store& store);
void paintShowcaseScene(
    glim::paint::Context& ctx,
    glim::toolkit::host::LayerFeed& feed,
    Store& store,
    int focus = -1,
    bool focusVisible = true
);

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

// Dashboard state lives in Scope cells; widgets read them by key.
struct Shown {
  State<int> taps;
  State<int> range;
  State<float> level;
  State<int> page;
  State<bool> theme;
  State<bool> live;
};

template <class W, class H>
H widgetState(
    const Block& tree, const Box& box, const Engine& e, H(W::*mem)
) {
  H out;
  bool found = false;
  walk(
      tree,
      box,
      e,
      [&](const Block& n, const Box&, const Engine&) {
        if (auto c = n.get<Custom>()) {
          if (auto x = c->get().as<W>()) {
            out = x->get().*mem;
            (void)*out;
            found = true;
            return false;
          }
        }
        return true;
      },
      [](const Block&, const Box&, const Engine&) {}
  );
  assert(found && "showcase tree is missing a widget");
  return out;
}

Shown shownStates(Store& store) {
  Scope s(store, "");
  return Shown{
      s.state("taps", 0),
      s.state("range", 1),
      s.state("level", 0.4f),
      s.state("page", 0),
      s.state("theme", false),
      s.state("live", true),
  };
}

}  // namespace

int main(int argc, char** argv) {
  // Headless eyeball: --paint out.png renders the full scene through
  // the software rasterizer, no window needed.
  if (argc > 2 && std::string(argv[1]) == "--paint") {
    glim::paint::Context ctx;
    // --paint out.png [page] [theme] [vscroll] [height]. Huge vscroll
    // clamps to max scroll, so short-viewport breathing is eyeballable.
    int paintH = 760;
    if (argc > 6) paintH = std::atoi(argv[6]);
    if (paintH < 200) paintH = 200;
    ctx.setSize({1240.0f, static_cast<float>(paintH)});
    ctx.beginFrame();
    host::LayerFeed feed;
    Store app;
    if (argc > 3) app.state("page", 0) = std::atoi(argv[3]);
    if (argc > 4) app.state("theme", false) = (std::atoi(argv[4]) != 0);
    if (argc > 5)
      app.state("vscroll", 0.0f) = std::strtof(argv[5], nullptr);
    paintShowcaseScene(ctx, feed, app);
    ctx.finish();
    auto img = glim::image::Image::rgba8(1240, paintH);
    glim::paint::rasterScene(
        ctx.scene(), 1240, paintH, img.rgba.data()
    );
    if (!glim::image::Png::write(argv[2], img)) {
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

  Store s;
  assert(showcaseDump(s).find("Nav") != std::string::npos);
  assert(showcaseDump(s).find("FRAME") != std::string::npos);
  assert(showcaseFocusDump(s).find("Nav") != std::string::npos);
  assert(!showcaseA11yDump(s).empty());
  // Determinism spot-check: the idle scene (null hub, no clock) must
  // render identically twice — telemetry panels show placeholders.
  {
    glim::paint::Context pctx;
    pctx.setSize({1024, 640});
    pctx.beginFrame();
    host::LayerFeed pfeed;
    Store pstore;
    paintShowcaseScene(pctx, pfeed, pstore, -1, false);
    pctx.finish();
    glim::image::Image a = glim::image::Image::rgba8(1024, 640);
    glim::paint::rasterScene(pctx.scene(), 1024, 640, a.rgba.data());
    glim::paint::Context qctx;
    qctx.setSize({1024, 640});
    qctx.beginFrame();
    host::LayerFeed qfeed;
    Store qstore;
    paintShowcaseScene(qctx, qfeed, qstore, -1, false);
    qctx.finish();
    glim::image::Image b = glim::image::Image::rgba8(1024, 640);
    glim::paint::rasterScene(qctx.scene(), 1024, 640, b.rgba.data());
    assert(a.rgba == b.rgba);
  }

  // T7 record gate: tree build + place + record into the Context
  // stays under 4 ms on this screen. The host loop reuses its Store,
  // LayerFeed, and Context across frames, so the gate does the same:
  // one-time bake costs (atlas, layer feed) warm up outside the
  // measurement; each sample rebuilds the tree fresh, like a frame
  // does. Min-of-N after warmup filters scheduling noise (CI runs
  // suites in parallel). GPU-side encode is a separate stage — its
  // live number is the telemetry encode row; the CPU software raster
  // above is full rasterization, not encode, so it is not part of
  // this gate.
  {
    using clk = std::chrono::steady_clock;
    glim::paint::Context ctx;
    ctx.setSize({1024, 640});
    host::LayerFeed feed;
    Store store;
    auto recordOnce = [&]() {
      auto t0 = clk::now();
      ctx.beginFrame();
      paintShowcaseScene(ctx, feed, store, -1, false);
      ctx.finish();
      auto t1 = clk::now();
      return std::chrono::duration<double, std::milli>(t1 - t0).count();
    };
    // First frames warm the one-time bake (atlas, layer feed); the
    // gate measures the steady state a live loop records in.
    for (int i = 0; i < 3; ++i) recordOnce();
    double best = recordOnce();
    for (int i = 1; i < 11; ++i) best = std::min(best, recordOnce());
    std::cout << "record " << best << " ms\n";
    assert(best < 4.0);
  }

  // Touch end to end on the dashboard. Overview page: nav x3,
  // display toggle, Live — one live-telemetry section, all panels
  // read-only, never focusable.
  Store t;
  Engine te = engineFor(t);
  Block tree = showcaseTree(rootScope(te));
  // Interaction runs in a viewport tall enough to show the whole
  // tree. Focus order and box sizes are unaffected, only y positions
  // shift with the taller fold.
  Box box = te.place(tree, {0, 1024, 0, 1600});
  Shown h = shownStates(t);
  auto focusableList = focusables(tree, box);
  // Focus order on Overview: nav x3 (rail), display toggle
  // (rail bottom), Live (pipeline strip).
  assert(focusableList.size() == 5);
  // Display toggle flips the ramp; the themed root resolves the
  // light card surface, dark back to the default ramp.
  {
    assert(*h.theme == false);
    auto paintCardTop = [&](bool light) {
      h.theme = light;
      Block st = showcaseTree(rootScope(te));
      Box db = te.place(st, {0, 1024, 0, 640});
      OpsRecorder rec;
      host::LayerFeed stubFeed;
      Engine e;
      paintTree(st, db, e, rec, stubFeed);
      std::uint32_t top = 0;
      for (const auto& o : rec.ops)
        if (o.kind == Op::Kind::FillGradient) {
          top = o.rgba;
          break;
        }
      return top;
    };
    assert(paintCardTop(false) == Show::kCardTop);
    // Rail-bottom display toggle: same Switch element as Live.
    Box th = focusBox(tree, box, 3);
    assert(th.w == 52 && th.h == 28);
    assert(dispatchPointer(
        tree,
        box,
        th.x + 1,
        th.y + 1,
        Event::borrow(PointerDown{th.x + 1, th.y + 1}),
        te
    ));
    assert(*h.theme == true);
    assert(paintCardTop(true) == ShowLight::kCardTop);
    assert(dispatchPointer(
        tree,
        box,
        th.x + 1,
        th.y + 1,
        Event::borrow(PointerDown{th.x + 1, th.y + 1}),
        te
    ));
    assert(*h.theme == false);
  }
  // Live switch flips the driver cell; the host arms/drops the
  // never-settling frame watch from it (Store mechanics pinned
  // below). Layout and focus order are untouched.
  {
    assert(*h.live == true);
    Box lv = focusBox(tree, box, 4);
    assert(lv.w == 52 && lv.h == 28);
    assert(dispatchPointer(
        tree,
        box,
        lv.x + 1,
        lv.y + 1,
        Event::borrow(PointerDown{lv.x + 1, lv.y + 1}),
        te
    ));
    assert(*h.live == false);
    assert(dispatchPointer(
        tree,
        box,
        lv.x + 1,
        lv.y + 1,
        Event::borrow(PointerDown{lv.x + 1, lv.y + 1}),
        te
    ));
    assert(*h.live == true);
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
  // Nav switches pages: tapping Controls rebuilds to the control
  // gallery, tapping Overview returns.
  {
    assert(*h.page == 0);
    Box nav = focusBox(tree, box, 1);
    assert(dispatchPointer(
        tree,
        box,
        nav.x + 1,
        nav.y + 1,
        Event::borrow(PointerDown{nav.x + 1, nav.y + 1})
    ));
    assert(*h.page == 1);
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
  // Pre-contact writes are preserved: a pending Anim stashes
  // sets, and the cell allocates with the latest value on contact.
  {
    Store m;
    Engine me = engineFor(m);
    Scope ms(m, "");
    auto v = ms.state("v", false);
    Anim<float> p("pp", 0.0f);
    p = 0.75f;
    assert(*p == 0.75f && !m.valuesLive());
    Block cbt = checkbox({.label = "S", .value = v, .pos = p});
    Box cb = place(cbt, {0, 720, 0, 480});
    assert(dispatchPointer(
        cbt, cb, 12, 12, Event::borrow(PointerDown{12, 12}), me
    ));
    auto custom = cbt.get<Custom>();
    assert(custom);
    auto cbx = custom->get().as<Checkbox>();
    assert(cbx);
    Anim<float> q = cbx->get().pos;
    assert(*v == true && *q == 0.75f && m.valuesLive());
    m.stepValues(0.0f);
    m.stepValues(1.0f);
    assert(*q == 1.0f && !q.live() && !m.valuesLive());
  }
  // Segmented indicator coasts between segments. The group itself
  // stays out of focus — the three segment taps own interaction
  // (no focusable added by the animation).
  {
    Store m;
    Engine me = engineFor(m);
    Scope ms(m, "");
    auto sel = ms.state("sel", 0);
    Anim<float> sp{ms, "sp", 0.0f};
    Segmented sgd;
    sgd.options = {"Day", "Week", "Month"};
    sgd.selected = sel;
    sgd.selPos = sp;
    Block sgt = segmented(std::move(sgd));
    Box sb = place(sgt, {0, 720, 0, 480});
    assert(focusables(sgt, sb, me).size() == 3);
    const float third = sb.w / 3.0f;
    assert(dispatchPointer(
        sgt,
        sb,
        sb.x + third * 1.5f,
        sb.y + 18,
        Event::borrow(PointerDown{sb.x + third * 1.5f, sb.y + 18})
    ));
    assert(*sel == 1 && *sp == 0.0f && m.valuesLive());
    m.stepValues(0.0f);
    m.stepValues(0.11f);
    assert(*sp > 0.0f && *sp < 1.0f);
    m.stepValues(1.0f);
    assert(*sp == 1.0f && !sp.live() && !m.valuesLive());
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
  // Mover: the wrapper composition demo. The child lays out shifted
  // by the Anim; no pump code in the element, the Store drives it.
  {
    Store m;
    Engine me = engineFor(m);
    Scope ms(m, "");
    Anim<float> dx{ms, "dx", 0.0f};
    Block mv = mover({.shift = dx}, lines("Panel"));
    Box mb = place(mv, {0, 720, 0, 480});
    Rec r0;
    me.record(mv, mb, r0);
    float x0 = 0.0f;
    for (const auto& e : r0.entries) {
      if (e.kind == "Lines") x0 = e.box.x;
    }
    dx = -100.0f;
    assert(m.valuesLive());
    m.stepValues(0.0f);
    m.stepValues(1.0f);
    assert(*dx == -100.0f && !m.valuesLive());
    Block mv2 = mover({.shift = dx}, lines("Panel"));
    Box mb2 = place(mv2, {0, 720, 0, 480});
    Rec r1;
    me.record(mv2, mb2, r1);
    bool shifted = false;
    for (const auto& e : r1.entries) {
      if (e.kind == "Lines" && e.box.x == x0 - 100.0f) shifted = true;
    }
    assert(shifted);
  }
  // Widget gallery lives on Controls: segmented, slider, and the
  // capture path are pinned against the Controls tree (page 1).
  {
    h.page = 1;
    Block ctree = showcaseTree(rootScope(te));
    Box cbox = te.place(ctree, {0, 1024, 0, 1600});
    // nav x3, display, export, cancel, seat, switch, checkbox,
    // segments x3, slider.
    assert(focusables(ctree, cbox).size() == 13);
    // Segmented moves selection per segment tap.
    {
      assert(*h.range == 1);
      Box sg = focusBox(ctree, cbox, 11);
      assert(dispatchPointer(
          ctree,
          cbox,
          sg.x + 1,
          sg.y + 1,
          Event::borrow(PointerDown{sg.x + 1, sg.y + 1})
      ));
      assert(*h.range == 2);
      h.range = 1;
    }
    // Slider: tap maps x to value through the laid-out box
    // (handleAt); a pressed drag scrubs, hover moves are ignored,
    // arrows nudge.
    {
      assert(*h.level > 0.39f && *h.level < 0.41f);
      Box sl = focusBox(ctree, cbox, 12);
      assert(sl.w == 240 && sl.h == 28);
      assert(dispatchPointer(
          ctree,
          cbox,
          sl.x + 10,
          sl.y + 14,
          Event::borrow(PointerDown{sl.x + 10, sl.y + 14})
      ));
      assert(*h.level == 0.0f);
      assert(dispatchPointer(
          ctree,
          cbox,
          sl.x + sl.w - 10,
          sl.y + 14,
          Event::borrow(PointerDown{sl.x + sl.w - 10, sl.y + 14})
      ));
      assert(*h.level == 1.0f);
      PointerMove hover{sl.x + 120, sl.y + 14, false};
      assert(!dispatchPointer(
          ctree, cbox, hover.x, hover.y, Event::borrow(hover)
      ));
      assert(*h.level == 1.0f);
      PointerMove drag{sl.x + 120, sl.y + 14, true};
      assert(dispatchPointer(
          ctree, cbox, drag.x, drag.y, Event::borrow(drag)
      ));
      assert(*h.level > 0.49f && *h.level < 0.51f);
      int ki = deliverKey(ctree, cbox, 12, Key::Right);
      assert(ki == 12 && *h.level > 0.59f && *h.level < 0.61f);
      h.level = 0.4f;
    }
    // Screen capture path (what the host loop actually drives):
    // down captures the slider, pressed moves route captured and must
    // flag a repaint via the store revision, or drags silently freeze.
    {
      Store u;
      u.state("page", 0) = 1;
      Engine ue = engineFor(u);
      Block stree = showcaseTree(rootScope(ue));
      Box sbox = ue.place(stree, {0, 1024, 0, 1600});
      Shown uh = shownStates(u);
      Screen scr;
      Box sl = focusBox(stree, sbox, 12, ue);
      bool downOk =
          scr.pointerDown(stree, sbox, sl.x + 10, sl.y + 14, 1, ue);
      assert(downOk);
      assert(*uh.level == 0.0f);
      assert(scr.needsPaint(u));
      scr.painted(u);
      assert(!scr.needsPaint(u));
      assert(
          scr.pointerMove(
              stree, sbox, sl.x + 10, sl.y + 14, true, 1, ue
          ) == false
      );
      assert(*uh.level == 0.0f);
      scr.painted(u);
      assert(
          scr.pointerMove(
              stree, sbox, sl.x + 120, sl.y + 14, true, 1, ue
          ) == false
      );
      assert(*uh.level > 0.49f && *uh.level < 0.51f);
      assert(scr.needsPaint(u));
    }
    // Leave the store on Controls for the block below.
    h.page = 1;
  }
  // Controls page (rebuild after the nav tap above): export/cancel,
  // switch + checkbox flip their cells on tap; seat anchor claims
  // focus for the native field (no toolkit action).
  {
    Block ctree = showcaseTree(rootScope(te));
    Box cbox = te.place(ctree, {0, 1024, 0, 1600});
    auto cfocus = focusables(ctree, cbox);
    // nav x3, display, export, cancel, seat, switch, checkbox,
    // segments x3, slider.
    assert(cfocus.size() == 13);
    // Clip contract, pinned in context: at a short 480px viewport
    // the slider lays out below the fold, so dispatch misses it
    // instead of actuating an invisible control. Reachable again
    // once the outer roll gets a scroll offset.
    {
      Box clipped = te.place(ctree, {0, 1024, 0, 480});
      Box sl = focusBox(ctree, clipped, 12);
      assert(sl.y >= 480);
      assert(!dispatchPointer(
          ctree,
          clipped,
          sl.x + 10,
          sl.y + 14,
          Event::borrow(PointerDown{sl.x + 10, sl.y + 14})
      ));
      assert(*h.level > 0.39f && *h.level < 0.41f);
      Screen ws;
      assert(ws.wheel(ctree, clipped, 500, 240, 0.0f, 5000.0f, te));
      State<float> vs = t.state("vscroll", 0.0f);
      const float got = *vs;
      // Max scroll is content-bound; the pin is that the shifted
      // point lands back inside the fold and actuates.
      assert(got > 0.0f);
      assert(sl.y + 14 - got < 480.0f);
      assert(dispatchPointer(
          ctree,
          clipped,
          sl.x + 10,
          sl.y + 14 - got,
          Event::borrow(PointerDown{sl.x + 10, sl.y + 14 - got})
      ));
      assert(*h.level == 0.0f);
      h.level = 0.4f;
      vs = 0.0f;
    }
    {
      Box ex = focusBox(ctree, cbox, 4);
      assert(dispatchPointer(
          ctree,
          cbox,
          ex.x + 1,
          ex.y + 1,
          Event::borrow(PointerDown{ex.x + 1, ex.y + 1})
      ));
      assert(*h.taps == 1);
      Box cx = focusBox(ctree, cbox, 5);
      assert(dispatchPointer(
          ctree,
          cbox,
          cx.x + 1,
          cx.y + 1,
          Event::borrow(PointerDown{cx.x + 1, cx.y + 1})
      ));
      assert(*h.taps == 0);
    }
    Box sw = focusBox(ctree, cbox, 7);
    assert(sw.w == 52 && sw.h == 28);
    State<bool> wifi =
        widgetState<Switch>(ctree, cbox, te, &Switch::value);
    assert(*wifi == true);
    assert(dispatchPointer(
        ctree,
        cbox,
        sw.x + 1,
        sw.y + 1,
        Event::borrow(PointerDown{sw.x + 1, sw.y + 1}),
        te
    ));
    assert(*wifi == false);
    State<bool> sound =
        widgetState<Checkbox>(ctree, cbox, te, &Checkbox::value);
    assert(*sound == false);
    Box cb = focusBox(ctree, cbox, 8);
    assert(dispatchPointer(
        ctree,
        cbox,
        cb.x + 1,
        cb.y + 1,
        Event::borrow(PointerDown{cb.x + 1, cb.y + 1}),
        te
    ));
    assert(*sound == true);
    Box seatFocus = focusBox(ctree, cbox, 6);
    assert(seatFocus.w == 600 && seatFocus.h == 40);
    assert(dispatchPointer(
        ctree,
        cbox,
        seatFocus.x + 1,
        seatFocus.y + 1,
        Event::borrow(PointerDown{seatFocus.x + 1, seatFocus.y + 1})
    ));
    // Back to Overview for the remaining pin checks.
    h.page = 0;
  }
  // Focus ring paints last on the screen record.
  {
    Box first2 = focusBox(tree, box, 0);
    Box rb = focusRingBox(tree, box, 0);
    assert(rb.x == first2.x - 2);
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

  // Bridge ops on Overview: the hero title strip is real glass —
  // a frosted bar floating over the trace, so the blur has varied
  // content behind it. Elements otherwise paint primitives (no
  // widget switch); gel/Plus lives on the Controls gallery only;
  // the archive round-trips byte-identically (rejects
  // truncation/magic).
  {
    Block screen = showcaseTree(rootScope(te));
    Box dbox = place(screen, {0, 1024, 0, 640});

    OpsRecorder rec;
    host::LayerFeed stubFeed;
    stubFeed.id = 7;
    stubFeed.foreign = true;
    {
      Engine e;
      paintTree(screen, dbox, e, rec, stubFeed);
      paintFocusRing(screen, dbox, 0, rec);
    }
    auto findOp = [&](Op::Kind k, int nth = 0) -> int {
      int seen = 0;
      for (std::size_t i = 0; i < rec.ops.size(); ++i)
        if (rec.ops[i].kind == k && seen++ == nth)
          return static_cast<int>(i);
      return -1;
    };
    int ring = -1;  // focus ring paints last, on top of everything
    for (std::size_t i = 0; i < rec.ops.size(); ++i)
      if (rec.ops[i].kind == Op::Kind::Stroke)
        ring = static_cast<int>(i);
    assert(ring >= 0);
    // Header glass is flat and dark-tinted; hero keeps the full edge.
    // No gel/Plus on the command deck.
    assert(findOp(Op::Kind::PushGlass, 0) >= 0);
    assert(findOp(Op::Kind::PushGlass, 1) >= 0);
    assert(findOp(Op::Kind::PushGlass, 2) < 0);
    {
      const Op& hero = rec.ops[static_cast<std::size_t>(
          findOp(Op::Kind::PushGlass, 0)
      )];
      const Op& header = rec.ops[static_cast<std::size_t>(
          findOp(Op::Kind::PushGlass, 1)
      )];
      assert(hero.edge == static_cast<std::uint8_t>(GlassEdge::Full));
      assert(hero.tint == 0);
      assert(header.edge == static_cast<std::uint8_t>(GlassEdge::None));
      assert(header.tint != 0);
    }
    assert(findOp(Op::Kind::PushBlend, 0) < 0);
    {
      int card = -1;
      for (std::size_t i = 0; i < rec.ops.size(); ++i)
        if (rec.ops[i].kind == Op::Kind::FillGradient &&
            rec.ops[i].rgba == Show::kCardTop &&
            rec.ops[i].rgba2 == Show::kCardBottom) {
          card = static_cast<int>(i);
          break;
        }
      assert(card >= 0);
      assert(card < ring);
    }

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
      Engine pe2;
      host::paintScreen(ctx, screen, dbox, pe2, feed, 0);
    }
    ctx.finish();
    const glim::paint::Scene& scene = ctx.scene();
    int glyphRuns = 0;
    int glassGroups = 0;
    std::vector<std::string> rootSeq;
    std::function<void(const glim::paint::Group&, int)> walkGroups =
        [&](const glim::paint::Group& g, int depth) {
          glim::paint::visitGroup(
              g,
              [&](const glim::paint::Shape& s) {
                if (std::holds_alternative<glim::paint::SlotHole>(s)) {
                  rootSeq.push_back("Slot");
                } else if (std::holds_alternative<glim::paint::Stroke>(s
                           )) {
                  rootSeq.push_back("Stroke");
                } else if (std::holds_alternative<
                               glim::paint::GlyphRun>(s)) {
                  ++glyphRuns;
                } else if (depth == 0) {
                  rootSeq.push_back("Fill");
                }
              },
              [&](const glim::paint::Group& c) {
                if (glim::paint::hasGlass(c)) {
                  if (depth == 0) rootSeq.push_back("Glass");
                  ++glassGroups;
                }
                walkGroups(c, depth + 1);
              }
          );
        };
    walkGroups(scene.root, 0);
    // Header + hero glass; glyphs everywhere.
    assert(glassGroups == 2);
    assert(glyphRuns > 0);
    // The focus ring is the LAST stroke on top of everything.
    int rRing = -1;
    for (std::size_t i = 0; i < rootSeq.size(); ++i)
      if (rootSeq[i] == "Stroke") rRing = static_cast<int>(i);
    assert(rRing >= 0);
  }

  // Bridge ops on Controls: slot < ring; the SlotHole is a root
  // sibling, never inside glass.
  {
    Store cs;
    Engine ce = engineFor(cs);
    cs.state("page", 0) = 1;
    Block screen = showcaseTree(rootScope(ce));
    Box dbox = place(screen, {0, 1024, 0, 640});

    OpsRecorder rec;
    host::LayerFeed stubFeed;
    stubFeed.id = 7;
    stubFeed.foreign = true;
    {
      Engine e;
      paintTree(screen, dbox, e, rec, stubFeed);
      paintFocusRing(screen, dbox, 0, rec);
    }
    auto findOp = [&](Op::Kind k, int nth = 0) -> int {
      int seen = 0;
      for (std::size_t i = 0; i < rec.ops.size(); ++i)
        if (rec.ops[i].kind == k && seen++ == nth)
          return static_cast<int>(i);
      return -1;
    };
    int slot = findOp(Op::Kind::Slot);
    int ring = -1;  // focus ring paints last, on top of everything
    for (std::size_t i = 0; i < rec.ops.size(); ++i)
      if (rec.ops[i].kind == Op::Kind::Stroke)
        ring = static_cast<int>(i);
    assert(slot >= 0 && ring >= 0);
    assert(slot < ring);
    assert(
        rec.ops[static_cast<std::size_t>(slot)].slot == kShowcaseSeatId
    );
    // Gel speculars ride a Plus scope on the primary Export button:
    // additive light, not SrcOver wash.
    assert(findOp(Op::Kind::PushBlend, 0) >= 0);
    {
      int grad = -1;
      for (std::size_t i = 0; i < rec.ops.size(); ++i)
        if (rec.ops[i].kind == Op::Kind::FillGradient &&
            rec.ops[i].rgba == Show::kButtonPrimaryTop &&
            rec.ops[i].rgba2 == Show::kButtonPrimaryBottom) {
          grad = static_cast<int>(i);
          break;
        }
      assert(grad >= 0);
    }
    // Seat reconcile: the slot box is the Seat's placed box — the
    // tree owns it, no harness-supplied rect.
    {
      Engine e2;
      Box seatBox{0, 0, 0, 0};
      bool sawSeat = false;
      walk(
          screen,
          dbox,
          e2,
          [&](const Block& n, const Box& bx, const Engine&) -> bool {
            if (auto s = n.get<Seat>()) {
              if (s->get().id == kShowcaseSeatId) {
                seatBox = bx;
                sawSeat = true;
              }
            }
            return true;
          },
          [](const Block&, const Box&, const Engine&) {}
      );
      assert(sawSeat);
      const Box& slotBox = rec.ops[static_cast<std::size_t>(slot)].box;
      assert(
          slotBox.x == seatBox.x && slotBox.y == seatBox.y &&
          slotBox.w == seatBox.w && slotBox.h == seatBox.h
      );
    }

    glim::paint::Context ctx;
    ctx.setSize({1024, 640});
    ctx.beginFrame();
    {
      host::LayerFeed feed;
      Engine pe2;
      host::paintScreen(ctx, screen, dbox, pe2, feed, 0);
    }
    ctx.finish();
    const glim::paint::Scene& scene = ctx.scene();
    bool slotInGlass = false;
    std::vector<std::string> rootSeq;
    std::function<void(const glim::paint::Group&, int, bool)>
        walkGroups = [&](const glim::paint::Group& g,
                         int depth,
                         bool inGlass) {
          glim::paint::visitGroup(
              g,
              [&](const glim::paint::Shape& s) {
                if (std::holds_alternative<glim::paint::SlotHole>(s)) {
                  rootSeq.push_back("Slot");
                  if (depth == 0) {
                    const auto& hole =
                        std::get<glim::paint::SlotHole>(s);
                    assert(hole.id == kShowcaseSeatId);
                  }
                  if (inGlass) slotInGlass = true;
                } else if (std::holds_alternative<glim::paint::Stroke>(s
                           )) {
                  rootSeq.push_back("Stroke");
                } else if (depth == 0) {
                  rootSeq.push_back("Fill");
                }
              },
              [&](const glim::paint::Group& c) {
                bool cg = glim::paint::hasGlass(c);
                if (cg) rootSeq.push_back("Glass");
                walkGroups(c, depth + 1, inGlass || cg);
              }
          );
        };
    walkGroups(scene.root, 0, false);
    assert(!slotInGlass);
    auto pos = [&](const char* k) -> int {
      for (std::size_t i = 0; i < rootSeq.size(); ++i)
        if (rootSeq[i] == k) return static_cast<int>(i);
      return -1;
    };
    int rSlot = pos("Slot");
    assert(rSlot >= 0);
    // Focus ring is the LAST stroke on top of everything.
    int rRing = -1;
    for (std::size_t i = 0; i < rootSeq.size(); ++i)
      if (rootSeq[i] == "Stroke") rRing = static_cast<int>(i);
    assert(rRing > rSlot);
  }

  // Bridge ops on System: one glass span (the floating header).
  {
    Store cs;
    Engine ce = engineFor(cs);
    cs.state("page", 0) = 2;
    Block screen = showcaseTree(rootScope(ce));
    Box dbox = place(screen, {0, 1024, 0, 640});

    OpsRecorder rec;
    host::LayerFeed stubFeed;
    stubFeed.id = 7;
    stubFeed.foreign = true;
    {
      Engine e;
      paintTree(screen, dbox, e, rec, stubFeed);
      paintFocusRing(screen, dbox, 0, rec);
    }
    auto findOp = [&](Op::Kind k, int nth = 0) -> int {
      int seen = 0;
      for (std::size_t i = 0; i < rec.ops.size(); ++i)
        if (rec.ops[i].kind == k && seen++ == nth)
          return static_cast<int>(i);
      return -1;
    };
    assert(findOp(Op::Kind::PushGlass, 0) >= 0);
    assert(findOp(Op::Kind::PushGlass, 1) < 0);
    // The one span is the header: flat, dark-tinted.
    {
      const Op& header = rec.ops[static_cast<std::size_t>(
          findOp(Op::Kind::PushGlass, 0)
      )];
      assert(header.edge == static_cast<std::uint8_t>(GlassEdge::None));
      assert(header.tint != 0);
    }
    assert(findOp(Op::Kind::Slot, 0) < 0);
    int ring = -1;
    for (std::size_t i = 0; i < rec.ops.size(); ++i)
      if (rec.ops[i].kind == Op::Kind::Stroke)
        ring = static_cast<int>(i);
    assert(ring >= 0);

    glim::paint::Context ctx;
    ctx.setSize({1024, 640});
    ctx.beginFrame();
    {
      host::LayerFeed feed;
      Engine pe2;
      host::paintScreen(ctx, screen, dbox, pe2, feed, 0);
    }
    ctx.finish();
    int glassGroups = 0;
    int glyphRuns = 0;
    std::function<void(const glim::paint::Group&)> walkSys =
        [&](const glim::paint::Group& g) {
          glim::paint::visitGroup(
              g,
              [&](const glim::paint::Shape& s) {
                if (std::holds_alternative<glim::paint::GlyphRun>(s))
                  ++glyphRuns;
              },
              [&](const glim::paint::Group& c) {
                if (glim::paint::hasGlass(c)) ++glassGroups;
                walkSys(c);
              }
          );
        };
    walkSys(ctx.scene().root);
    // One glass span (header); glyphs still present.
    assert(glassGroups == 1);
    assert(glyphRuns > 0);
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

  // Widget data helpers: strip placeholders, alert parsing, log
  // classification, stage rows.
  {
    assert(stripMs(nullptr).size() == 16);
    host::TelemetryHub empty(8);
    assert(stripMs(&empty).size() == 16);
    assert(hubStages(&empty).size() == 4);
    assert(ringValue(&empty) > 0.44f && ringValue(&empty) < 0.46f);
    host::FrameTelemetry hot{};
    hot.frameMs = 10.0f;
    empty.push(hot);
    assert(stripMs(&empty).size() == 1);
    assert(stripMs(&empty).front() == 10.0f);
    assert(hubStages(&empty).size() == 4);
    assert(hubStages(&empty).front().ms == 10.0f);
    assert(ringValue(&empty) > 0.29f && ringValue(&empty) < 0.31f);

    std::vector<std::string> log = {
        "Frame |t 0.1s", "~ Slow |t 0.2s", "! HITCH |t 0.3s"
    };
    auto cards = alertCards(log);
    assert(cards.size() == 3);
    // Newest first: HITCH, Slow, Frame.
    assert(cards[0].severity == 2 && cards[0].title == "HITCH");
    assert(cards[1].severity == 1 && cards[1].title == "Slow");
    assert(cards[2].severity == 0 && cards[2].title == "Frame");

    host::FrameTelemetry ok{};
    ok.frameMs = 5.0f;
    assert(telemetryLogLine(ok).rfind("Frame |", 0) == 0);
    host::FrameTelemetry warn{};
    warn.frameMs = 22.0f;
    assert(telemetryLogLine(warn).rfind("~ Slow |", 0) == 0);
    host::FrameTelemetry enc{};
    enc.frameMs = 5.0f;
    enc.encodeMs = 9.0f;
    assert(telemetryLogLine(enc).rfind("~ Slow |", 0) == 0);
    host::FrameTelemetry hitch{};
    hitch.frameMs = 40.0f;
    assert(telemetryLogLine(hitch).rfind("! HITCH |", 0) == 0);
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

  std::cout << "showcase_test ok\n";
  return EXIT_SUCCESS;
}
