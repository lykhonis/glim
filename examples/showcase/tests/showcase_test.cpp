#include "Showcase.h"

#include <glim/image/Golden.h>
#include <glim/image/Image.h>
#include <glim/image/Png.h>
#include <glim/paint/Context.h>
#include <glim/paint/Scene.h>
#include <glim/paint/Software.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "glim/toolkit/Toolkit.h"
#include "glim/toolkit/host/Codec.h"
#include "glim/toolkit/host/Host.h"
#include "glim/toolkit/host/Paint.h"
#include "widgets/ShowcaseTheme.h"

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

bool fileExists(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) {
    return false;
  }
  std::fclose(f);
  return true;
}

std::string readFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream os;
  os << in.rdbuf();
  return os.str();
}

int checkGolden(
    const std::string& dir,
    const std::string& name,
    const std::string& got
) {
  std::string path = dir + "/" + name;
  if (!fileExists(path)) {
    if (glim::image::goldenStrict()) {
      std::cerr << "missing golden " << path << " (strict mode)\n";
      return EXIT_FAILURE;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
      std::cerr << "could not write golden " << path << '\n';
      return EXIT_FAILURE;
    }
    out << got;
    std::cout << "wrote golden " << path << '\n';
    return EXIT_SUCCESS;
  }
  if (got != readFile(path)) {
    std::cerr << "golden mismatch: dump differs from " << path << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int checkPngGolden(
    const std::string& dir,
    const std::string& name,
    const glim::image::Image& got,
    const char* argv0
) {
  std::string path = dir + "/" + name;
  if (!fileExists(path)) {
    if (glim::image::goldenStrict()) {
      std::cerr << "missing golden " << path << " (strict mode)\n";
      return EXIT_FAILURE;
    }
    if (!glim::image::Png::write(path.c_str(), got)) {
      std::cerr << "could not write golden " << path << '\n';
      return EXIT_FAILURE;
    }
    std::cout << "wrote golden " << path << '\n';
    return EXIT_SUCCESS;
  }
  glim::image::Image want;
  if (!glim::image::Png::read(path.c_str(), &want)) {
    std::cerr << "could not read golden " << path << '\n';
    return EXIT_FAILURE;
  }
  if (!glim::image::checkCpuGolden(got, want, argv0, "showcase"))
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

Engine engineFor(Store& store) {
  Engine e = host::measureEngine();
  e.store = &store;
  return e;
}

// State handles re-open the same scoped keys the tree allocated,
// so tests observe the cells the tree captured.
struct Shown {
  State<int> taps;
  State<bool> wifi;
  State<bool> sound;
  State<int> range;
  State<float> level;
};

Shown shownStates(Store& store) {
  Scope s(&store, "");
  return Shown{
      s.state("taps", 0),
      s.state("wifi", true),
      s.state("sound", false),
      s.state("range", 1),
      s.state("level", 0.4f),
  };
}

}  // namespace

int main(int argc, char** argv) {
  // Headless eyeball: --paint out.png renders the full scene through
  // the software rasterizer, no window needed.
  if (argc > 2 && std::string(argv[1]) == "--paint") {
    glim::paint::Context ctx;
    ctx.setSize({720, 480});
    ctx.beginFrame();
    host::LayerFeed feed;
    Store app;
    paintShowcaseScene(ctx, feed, app);
    ctx.finish();
    auto img = glim::image::Image::rgba8(720, 480);
    glim::paint::rasterScene(ctx.scene(), 720, 480, img.rgba.data());
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

  std::string dir = argc > 1 ? argv[1] : "examples/showcase/golden";
  Store s;
  assert(showcaseDump(s).find("Seat") != std::string::npos);
  if (int rc = checkGolden(dir, "dump.txt", showcaseDump(s))) return rc;
  if (int rc = checkGolden(dir, "focus.txt", showcaseFocusDump(s)))
    return rc;
  if (int rc = checkGolden(dir, "a11y.txt", showcaseA11yDump(s)))
    return rc;
  // Asserted CPU raster gate (harness H1): the same scene --paint
  // writes, compared strictly against golden/showcase.png. Fresh
  // store, no focus ring, overlay off — deterministic by
  // construction (no clock, no stats overlay in the paint path).
  {
    glim::paint::Context pctx;
    pctx.setSize({720, 480});
    pctx.beginFrame();
    host::LayerFeed pfeed;
    Store pstore;
    paintShowcaseScene(pctx, pfeed, pstore, -1, false);
    pctx.finish();
    glim::image::Image pgot = glim::image::Image::rgba8(720, 480);
    glim::paint::rasterScene(pctx.scene(), 720, 480, pgot.rgba.data());
    if (int rc = checkPngGolden(dir, "showcase.png", pgot, argv[0]))
      return rc;
  }

  // Touch end to end on the real screen. Two buttons, seat anchor,
  // switch, checkbox, three segments, slider: progress and labels are
  // read-only, never focusable.
  Store t;
  Engine te = engineFor(t);
  Block tree = showcaseTree(rootScope(te));
  Shown h = shownStates(t);
  // Interaction runs in a viewport tall enough to show the whole
  // tree (the 480px golden viewport clips Segmented and below — see
  // the pinned miss below). Focus order and box sizes are unaffected,
  // only y positions shift with the taller fold.
  Box box = place(tree, {0, 720, 0, 1200});
  auto focusableList = focusables(tree, box);
  assert(focusableList.size() == 9);
  // Clip contract, pinned in context: at the 480px window the
  // segmented row lays out below the fold, so dispatch misses it
  // instead of actuating an invisible control. Reachable again once
  // the outer roll gets a scroll offset (see input-scroll-notes).
  {
    Box clipped = place(tree, {0, 720, 0, 480});
    Box sg = focusBox(tree, clipped, 7);
    assert(sg.y >= 480);
    assert(!dispatchPointer(
        tree,
        clipped,
        sg.x + 1,
        sg.y + 1,
        Event::borrow(PointerDown{sg.x + 1, sg.y + 1})
    ));
    assert(*h.range == 1);
    // Wheel scrolls the outer roll: the offset moves, and the same
    // segmented row becomes reachable at its shifted viewport point.
    Screen ws;
    assert(ws.wheel(tree, clipped, 360, 240, 0.0f, 300.0f, te));
    State<float> vs = t.state("vscroll", 0.0f);
    assert(*vs == 300.0f);
    assert(dispatchPointer(
        tree,
        clipped,
        sg.x + 1,
        sg.y + 1 - 300.0f,
        Event::borrow(PointerDown{sg.x + 1, sg.y + 1 - 300.0f})
    ));
    assert(*h.range == 2);
    // Reset the probed states: later blocks reuse this store/tree at
    // zero offset with the default selection.
    h.range = 1;
    vs = 0.0f;
  }
  // Primary counts up, secondary resets.
  {
    assert(*h.taps == 0);
    Box ex = focusBox(tree, box, 0);
    assert(dispatchPointer(
        tree,
        box,
        ex.x + 1,
        ex.y + 1,
        Event::borrow(PointerDown{ex.x + 1, ex.y + 1})
    ));
    assert(*h.taps == 1);
  }
  {
    Box cx = focusBox(tree, box, 1);
    assert(dispatchPointer(
        tree,
        box,
        cx.x + 1,
        cx.y + 1,
        Event::borrow(PointerDown{cx.x + 1, cx.y + 1})
    ));
    assert(*h.taps == 0);
  }
  // Switch + checkbox flip their cells on tap; the segmented row
  // moves selection per segment tap.
  {
    assert(*h.wifi == true);
    Box sw = focusBox(tree, box, 3);
    assert(sw.w == 52 && sw.h == 28);
    assert(dispatchPointer(
        tree,
        box,
        sw.x + 1,
        sw.y + 1,
        Event::borrow(PointerDown{sw.x + 1, sw.y + 1})
    ));
    assert(*h.wifi == false);
  }
  {
    assert(*h.sound == false);
    Box cb = focusBox(tree, box, 4);
    assert(dispatchPointer(
        tree,
        box,
        cb.x + 1,
        cb.y + 1,
        Event::borrow(PointerDown{cb.x + 1, cb.y + 1})
    ));
    assert(*h.sound == true);
  }
  {
    assert(*h.range == 1);
    Box sg = focusBox(tree, box, 7);
    assert(dispatchPointer(
        tree,
        box,
        sg.x + 1,
        sg.y + 1,
        Event::borrow(PointerDown{sg.x + 1, sg.y + 1})
    ));
    assert(*h.range == 2);
  }
  // Slider: tap maps x to value through the laid-out box (handleAt);
  // a pressed drag scrubs, hover moves are ignored, arrows nudge.
  {
    assert(*h.level > 0.39f && *h.level < 0.41f);
    Box sl = focusBox(tree, box, 8);
    assert(sl.w == 240 && sl.h == 28);
    assert(dispatchPointer(
        tree,
        box,
        sl.x + 10,
        sl.y + 14,
        Event::borrow(PointerDown{sl.x + 10, sl.y + 14})
    ));
    assert(*h.level == 0.0f);
    assert(dispatchPointer(
        tree,
        box,
        sl.x + sl.w - 10,
        sl.y + 14,
        Event::borrow(PointerDown{sl.x + sl.w - 10, sl.y + 14})
    ));
    assert(*h.level == 1.0f);
    PointerMove hover{sl.x + 120, sl.y + 14, false};
    assert(!dispatchPointer(
        tree, box, hover.x, hover.y, Event::borrow(hover)
    ));
    assert(*h.level == 1.0f);
    PointerMove drag{sl.x + 120, sl.y + 14, true};
    assert(
        dispatchPointer(tree, box, drag.x, drag.y, Event::borrow(drag))
    );
    assert(*h.level > 0.49f && *h.level < 0.51f);
    int ki = deliverKey(tree, box, 8, Key::Right);
    assert(ki == 8 && *h.level > 0.59f && *h.level < 0.61f);
  }
  // Screen capture path (what the host loop actually drives):
  // down captures the slider, pressed moves route captured and must
  // flag a repaint via the store revision, or drags silently freeze.
  {
    Store u;
    Engine ue = engineFor(u);
    Block stree = showcaseTree(rootScope(ue));
    Box sbox = place(stree, {0, 720, 0, 1200});
    Shown uh = shownStates(u);
    Screen scr;
    Box sl = focusBox(stree, sbox, 8);
    assert(scr.pointerDown(stree, sbox, sl.x + 10, sl.y + 14, 1, ue));
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
  // Seat anchor sits under the buttons; tapping it claims
  // focus for the native field (no toolkit action).
  {
    Box seatFocus = focusBox(tree, box, 2);
    assert(seatFocus.w == 600 && seatFocus.h == 40);
    assert(dispatchPointer(
        tree,
        box,
        seatFocus.x + 1,
        seatFocus.y + 1,
        Event::borrow(PointerDown{seatFocus.x + 1, seatFocus.y + 1})
    ));
  }
  // Ring overlay rides last on the screen record.
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

  // Bridge ops: slot < ring, elements paint primitives (no widget
  // switch); the frosted glass card ships one PushGlass group (veil
  // paints, backdrop melts, children stay sharp); the archive
  // round-trips byte-identically (rejects truncation/magic); the
  // SlotHole is a root sibling.
  {
    Block screen = showcaseTree(rootScope(te));
    Box dbox = place(screen, {0, 720, 0, 480});

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
    assert(findOp(Op::Kind::PushGlass, 0) >= 0);
    // Gel speculars ride a Plus scope: additive light, not SrcOver
    // wash. The archive round-trip below covers the new op codec.
    assert(findOp(Op::Kind::PushBlend, 0) >= 0);
    assert(
        rec.ops[static_cast<std::size_t>(slot)].slot == kShowcaseSeatId
    );
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
    }
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
            if (const Seat* s = n.get<Seat>()) {
              if (s->id == kShowcaseSeatId) {
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

    std::vector<std::uint8_t> bytes;
    host::encodeOps(rec.ops, bytes);
    std::vector<Op> back;
    assert(host::decodeOps(bytes.data(), bytes.size(), back));
    assert(back == rec.ops);
    assert(!host::decodeOps(bytes.data(), bytes.size() - 1, back));
    bytes[3] ^= 0xff;  // corrupt magic
    assert(!host::decodeOps(bytes.data(), bytes.size(), back));

    glim::paint::Context ctx;
    ctx.setSize({720, 480});
    ctx.beginFrame();
    {
      host::LayerFeed feed;
      Engine pe2;
      host::paintScreen(ctx, screen, dbox, pe2, feed, 0);
    }
    ctx.finish();
    const glim::paint::Scene& scene = ctx.scene();
    bool slotInGlass = false;
    int glyphRuns = 0;
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
                } else if (std::holds_alternative<
                               glim::paint::GlyphRun>(s)) {
                  ++glyphRuns;
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
    assert(glyphRuns > 0);
    auto pos = [&](const char* k) -> int {
      for (std::size_t i = 0; i < rootSeq.size(); ++i)
        if (rootSeq[i] == k) return static_cast<int>(i);
      return -1;
    };
    int rSlot = pos("Slot");
    assert(rSlot >= 0);
    // Icon rings stroke inline; the focus ring is the LAST stroke
    // on top of everything.
    int rRing = -1;
    for (std::size_t i = 0; i < rootSeq.size(); ++i)
      if (rootSeq[i] == "Stroke") rRing = static_cast<int>(i);
    assert(rRing > rSlot);
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
