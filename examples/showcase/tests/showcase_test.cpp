#include "Showcase.h"

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
#include "paint/Bridge.h"
#include "paint/Keys.h"
#include "paint/Ops.h"
#include "paint/Overlay.h"
#include "paint/VideoFeed.h"

using namespace glim::toolkit;
using namespace glim::toolkit::paint;

std::string showcaseDump(Showcase s);
std::string showcaseFocusDump(Showcase s);
Block showcaseTree(Showcase s);
void paintShowcaseScene(
    glim::paint::Context& ctx,
    VideoFeed& feed,
    Showcase s,
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

Showcase makeShown() {
  static Store store;
  return Showcase{
      store.make<std::string>("12+3"),
      store.make<bool>(true),
      store.make<bool>(false),
      store.make<int>(1),
      store.make<float>(0.4f),
      store.make<int>(0),
      store.make<int>(0),
      store.make<int>(0),
      store.make<int>(0),
      store.make<std::string>("")
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
    VideoFeed feed;
    paintShowcaseScene(ctx, feed, makeShown());
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
  assert(showcaseKey(glim::shell::Key::DpadLeft) == Key::Left);
  assert(showcaseKey(glim::shell::Key::DpadRight) == Key::Right);
  assert(showcaseKey(glim::shell::Key::DpadUp) == Key::Up);
  assert(showcaseKey(glim::shell::Key::DpadDown) == Key::Down);
  assert(showcaseKey(glim::shell::Key::Enter) == Key::Activate);
  assert(showcaseKey(glim::shell::Key::DpadCenter) == Key::Activate);
  assert(!showcaseKey(glim::shell::Key::Escape));
  assert(!showcaseKey(glim::shell::Key::Unknown));

  std::string dir = argc > 1 ? argv[1] : "examples/showcase/golden";
  Showcase s = makeShown();
  assert(showcaseDump(s).find("Seat") != std::string::npos);
  if (int rc = checkGolden(dir, "dump.txt", showcaseDump(s))) return rc;
  if (int rc = checkGolden(dir, "focus.txt", showcaseFocusDump(s)))
    return rc;

  // Touch end to end on the real screen. First focusable is the "7"
  // button; firing it appends through the cell.
  Store store;
  Showcase t{
      store.make<std::string>("12+3"),
      store.make<bool>(true),
      store.make<bool>(false),
      store.make<int>(1),
      store.make<float>(0.4f),
      store.make<int>(0),
      store.make<int>(0),
      store.make<int>(0),
      store.make<int>(0),
      store.make<std::string>("")
  };
  Block tree = showcaseTree(t);
  Box box = place(tree, {0, 720, 0, 480});
  auto focusableList = focusables(tree, box);
  // Range 3 + toolbar 6 + mode 6 + seat + switch + checkbox +
  // 3 segments + slider (progress, avatar, stats, dim labels are
  // read-only, never focusable).
  assert(focusableList.size() == 22);
  // Range chevrons shift the month window; download/export/bell
  // set the toolbar notice; tabs move selection.
  {
    assert(*t.span == 0);
    Box prev = focusBox(tree, box, 0);
    assert(dispatchPointer(
        tree,
        box,
        prev.x + 1,
        prev.y + 1,
        Event::borrow(PointerDown{prev.x + 1, prev.y + 1})
    ));
    assert(*t.span == -1);
  }
  {
    Box dl = focusBox(tree, box, 2);
    assert(dispatchPointer(
        tree,
        box,
        dl.x + 1,
        dl.y + 1,
        Event::borrow(PointerDown{dl.x + 1, dl.y + 1})
    ));
    assert(*t.notice == "Download started");
  }
  {
    Box ex = focusBox(tree, box, 4);
    assert(dispatchPointer(
        tree,
        box,
        ex.x + 1,
        ex.y + 1,
        Event::borrow(PointerDown{ex.x + 1, ex.y + 1})
    ));
    assert(*t.notice == "Exported 10,367 rows");
  }
  {
    assert(*t.tabSel == 0);
    Box tb = focusBox(tree, box, 14);
    assert(dispatchPointer(
        tree,
        box,
        tb.x + 1,
        tb.y + 1,
        Event::borrow(PointerDown{tb.x + 1, tb.y + 1})
    ));
    assert(*t.tabSel == 2);
  }
  Box first = focusBox(tree, box, 12);
  assert(dispatchPointer(
      tree,
      box,
      first.x + 1,
      first.y + 1,
      Event::borrow(PointerDown{first.x + 1, first.y + 1})
  ));
  assert(*t.tabSel == 0);
  assert(moveFocus(tree, box, 12, FocusDir::Right) == 13);
  {
    int ki = deliverKey(tree, box, 12, Key::Right);
    assert(ki == 13);
    ki = deliverKey(tree, box, ki, Key::Activate);
    assert(ki == 13 && *t.tabSel == 1);
  }
  // Switch + checkbox flip their cells on tap; the segmented row
  // moves selection per segment tap.
  {
    assert(*t.wifi == true);
    Box sw = focusBox(tree, box, 16);
    assert(sw.w == 52 && sw.h == 28);
    assert(dispatchPointer(
        tree,
        box,
        sw.x + 1,
        sw.y + 1,
        Event::borrow(PointerDown{sw.x + 1, sw.y + 1})
    ));
    assert(*t.wifi == false);
  }
  {
    assert(*t.sound == false);
    Box cb = focusBox(tree, box, 17);
    assert(dispatchPointer(
        tree,
        box,
        cb.x + 1,
        cb.y + 1,
        Event::borrow(PointerDown{cb.x + 1, cb.y + 1})
    ));
    assert(*t.sound == true);
  }
  {
    assert(*t.range == 1);
    Box sg = focusBox(tree, box, 20);
    assert(dispatchPointer(
        tree,
        box,
        sg.x + 1,
        sg.y + 1,
        Event::borrow(PointerDown{sg.x + 1, sg.y + 1})
    ));
    assert(*t.range == 2);
  }
  // Slider: tap maps x to value through the laid-out box (handleAt);
  // a pressed drag scrubs, hover moves are ignored, arrows nudge.
  {
    assert(*t.level > 0.39f && *t.level < 0.41f);
    Box sl = focusBox(tree, box, 21);
    assert(sl.w == 240 && sl.h == 28);
    assert(dispatchPointer(
        tree,
        box,
        sl.x + 10,
        sl.y + 14,
        Event::borrow(PointerDown{sl.x + 10, sl.y + 14})
    ));
    assert(*t.level == 0.0f);
    assert(dispatchPointer(
        tree,
        box,
        sl.x + sl.w - 10,
        sl.y + 14,
        Event::borrow(PointerDown{sl.x + sl.w - 10, sl.y + 14})
    ));
    assert(*t.level == 1.0f);
    PointerMove hover{sl.x + 120, sl.y + 14, false};
    assert(!dispatchPointer(
        tree, box, hover.x, hover.y, Event::borrow(hover)
    ));
    assert(*t.level == 1.0f);
    PointerMove drag{sl.x + 120, sl.y + 14, true};
    assert(
        dispatchPointer(tree, box, drag.x, drag.y, Event::borrow(drag))
    );
    assert(*t.level > 0.49f && *t.level < 0.51f);
    int ki = deliverKey(tree, box, 21, Key::Right);
    assert(ki == 21 && *t.level > 0.59f && *t.level < 0.61f);
  }
  // Seat anchor is focusable after the tab row; tapping it claims
  // focus for the native field (no toolkit action).
  {
    Box seatFocus = focusBox(tree, box, 15);
    assert(seatFocus.w == 664 && seatFocus.h == 40);
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
    Rec recr;
    record(tree, box, recr);
    std::size_t base = recr.entries.size();
    recordFocusRing(recr, tree, box, 0);
    assert(recr.entries.size() == base + 1);
    assert(recr.entries.back().kind == kFocusRingKind);
    Box first2 = focusBox(tree, box, 0);
    assert(recr.entries.back().box.x == first2.x - 2);
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

  // Bridge ops: slot < ring, all from the tree (Seat paints the
  // slot via AnySink); the archive round-trips byte-identically
  // (rejects truncation/magic); the SlotHole is a root sibling.
  // (The video-under-glass ordering proof lives in git history —
  // the hero no longer ships a video plate or frosted card.)
  {
    Block screen = showcaseTree(t);
    Box dbox = place(screen, {0, 720, 0, 480});

    OpsRecorder rec;
    VideoFeed stubFeed;
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
    auto findEntry = [&](const char* kind, int nth = 0) -> int {
      int seen = 0;
      for (std::size_t i = 0; i < rec.ops.size(); ++i)
        if (rec.ops[i].kind == Op::Kind::Entry &&
            rec.ops[i].entry.kind == kind && seen++ == nth)
          return static_cast<int>(i);
      return -1;
    };
    int slot = findOp(Op::Kind::Slot);
    int ring = findOp(Op::Kind::Stroke);
    assert(slot >= 0 && ring >= 0);
    assert(slot < ring);
    assert(findOp(Op::Kind::PushGlass, 0) < 0);  // no glass in hero
    assert(
        rec.ops[static_cast<std::size_t>(slot)].slot == kShowcaseSeatId
    );
    assert(findEntry("Card") >= 0 && findEntry("Button.primary") >= 0);
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
          [&](const Block& n, const Box& bx) -> bool {
            if (const Seat* s = n.get<Seat>()) {
              if (s->id == kShowcaseSeatId) {
                seatBox = bx;
                sawSeat = true;
              }
            }
            return true;
          },
          [](const Block&, const Box&) {}
      );
      assert(sawSeat);
      const Box& slotBox = rec.ops[static_cast<std::size_t>(slot)].box;
      assert(
          slotBox.x == seatBox.x && slotBox.y == seatBox.y &&
          slotBox.w == seatBox.w && slotBox.h == seatBox.h
      );
    }

    std::vector<std::uint8_t> bytes;
    encodeOps(rec.ops, bytes);
    std::vector<Op> back;
    assert(decodeOps(bytes.data(), bytes.size(), back));
    assert(back == rec.ops);
    assert(!decodeOps(bytes.data(), bytes.size() - 1, back));
    bytes[3] ^= 0xff;  // corrupt magic
    assert(!decodeOps(bytes.data(), bytes.size(), back));

    glim::paint::Context ctx;
    ctx.setSize({720, 480});
    ctx.beginFrame();
    {
      VideoFeed feed;
      ContextSink sink{ctx};
      paintShowcaseInto(screen, dbox, sink, feed);
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
    VideoFeed stubFeed;
    Engine pe;
    paintTree(probe, pbox, pe, prec, stubFeed);
    assert(prec.ops.size() == 1 && prec.ops[0].kind == Op::Kind::Entry);
    assert(prec.ops[0].entry.kind == "Probe");
    std::vector<std::uint8_t> pbytes;
    encodeOps(prec.ops, pbytes);
    std::vector<Op> pback;
    assert(decodeOps(pbytes.data(), pbytes.size(), pback));
    assert(pback == prec.ops);
  }
  {
    Block cp = toBlock(CustomPainted{});
    Box cbox = place(cp, {0, 720, 0, 480});
    glim::paint::Context pctx;
    pctx.setSize({720, 480});
    pctx.beginFrame();
    {
      VideoFeed pf;
      ContextSink ps{pctx};
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
      VideoFeed pf;
      Engine pe;
      paintTree(cp, cbox, pe, cr, pf);
    }
    assert(cr.ops.size() == 1 && cr.ops[0].kind == Op::Kind::Stroke);
    assert(cr.ops[0].width == 1.0f);
  }

  std::cout << "showcase_test ok\n";
  return EXIT_SUCCESS;
}
