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
#include "paint/Ops.h"
#include "paint/Overlay.h"
#include "paint/VideoFeed.h"

using namespace glim::toolkit;
using namespace glim::toolkit::paint;

std::string showcaseDump(Showcase s);
std::string showcaseModalDump(Showcase s);
std::string showcaseFocusDump(Showcase s);
Block showcaseTree(Showcase s);
Block showcaseModal(Showcase s);
void paintShowcaseScene(
    glim::paint::Context& ctx, VideoFeed& feed, Showcase s
);

namespace {

// Zero-codec peer: a brand-new kind never seen by Ops.h rides the
// generic entry codec untouched.
struct Probe {
  const char* kind() const { return "Probe"; }
  Box place(Constraints c, const Engine&) const {
    return glim::toolkit::detail::resolveSize(10.0f, 10.0f, c);
  }
  void record(const Box& box, Rec& rec, const Engine&) const {
    rec.push("Probe", box, "probe", "mark", 3);
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
  return Showcase{store.make<std::string>("12+3"), store.make<int>(0)};
}

}  // namespace

int main(int argc, char** argv) {
  // Headless eyeball: --paint out.png renders the full scene (screen +
  // modal overlay) through the software rasterizer, no window needed.
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

  std::string dir = argc > 1 ? argv[1] : "examples/showcase/golden";
  Showcase s = makeShown();
  if (int rc = checkGolden(dir, "dump.txt", showcaseDump(s))) return rc;
  if (int rc = checkGolden(dir, "modal.txt", showcaseModalDump(s)))
    return rc;
  if (int rc = checkGolden(dir, "focus.txt", showcaseFocusDump(s)))
    return rc;

  // Touch end to end on the real screen. First focusable is the "7"
  // button; firing it appends through the cell.
  Store store;
  Showcase t{store.make<std::string>("12+3"), store.make<int>(0)};
  Block tree = showcaseTree(t);
  Box box = place(tree, {0, 720, 0, 480});
  auto focusableList = focusables(tree, box);
  assert(focusableList.size() == 5);
  Box first = focusBox(tree, box, 0);
  assert(dispatchPointer(
      tree,
      box,
      first.x + 1,
      first.y + 1,
      Event::borrow(PointerDown{first.x + 1, first.y + 1})
  ));
  assert(*t.out == "12+37");
  // Spatial moves walk the screen top to bottom.
  assert(moveFocus(tree, box, 0, FocusDir::Right) == 1);
  assert(moveFocus(tree, box, 1, FocusDir::Right) == 2);
  // Keys drive the same walk; Activate handles the focused digit.
  {
    int ki = deliverKey(tree, box, 0, Key::Right);
    assert(ki == 1);
    std::string before = *t.out;
    ki = deliverKey(tree, box, ki, Key::Activate);
    assert(ki == 1 && *t.out == before + "8");
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

  // Modal overlay: veil fills the box, card centers via fills. Buttons
  // composed inside the Card stay reachable through eachChild.
  Block modal = showcaseModal(t);
  Box mbox = place(modal, {0, 720, 0, 480});
  const Stack* st2 = modal.get<Stack>();
  assert(st2 && st2->children.size() == 2);
  auto modalFocus = focusables(modal, mbox);
  assert(modalFocus.size() == 2);
  Box yesBox = focusBox(modal, mbox, 1);
  const Block* yesHit = hit(modal, mbox, yesBox.x + 1, yesBox.y + 1);
  assert(yesHit && yesHit->get<Custom>());
  assert(dispatchPointer(
      modal,
      mbox,
      yesBox.x + 1,
      yesBox.y + 1,
      Event::borrow(PointerDown{yesBox.x + 1, yesBox.y + 1})
  ));
  assert(*t.answer == 2);
  assert(moveFocus(modal, mbox, 0, FocusDir::Right) == 1);
  // Veil corner hits the veil, which ignores the event.
  const Block* veilHit = hit(modal, mbox, 10, 10);
  assert(
      veilHit && !handle(*veilHit, Event::borrow(PointerDown{10, 10}))
  );
  // Card padding resolves to the Card, which ignores the event.
  {
    Rec recr;
    record(modal, mbox, recr);
    Box cardBox{0, 0, 0, 0};
    bool foundCard = false;
    for (const auto& e : recr.entries)
      if (e.kind == "Card") {
        cardBox = e.box;
        foundCard = true;
      }
    assert(foundCard);
    const Block* padHit =
        hit(modal, mbox, cardBox.x + 1, cardBox.y + 1);
    assert(
        padHit && padHit->get<Custom>() &&
        !handle(
            *padHit,
            Event::borrow(PointerDown{cardBox.x + 1, cardBox.y + 1})
        )
    );
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

  // Bridge ops: Entry(Video) < glass < slot < ring; archive
  // round-trips byte-identically (rejects truncation/magic); Scene
  // has the Blit first, Clear glass groups consecutive, SlotHole as
  // a root sibling — never inside glass.
  {
    Block screen = showcaseTree(t);
    Block over = showcaseModal(t);
    Box dbox = place(screen, {0, 720, 0, 480});

    OpsRecorder rec;
    VideoFeed stubFeed;
    stubFeed.id = 7;
    stubFeed.foreign = true;
    {
      Engine e;
      paintTree(screen, dbox, e, rec, stubFeed);
      paintTree(over, dbox, e, rec, stubFeed);
      rec.slot(showcaseSeatBox(), kShowcaseSeatId);
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
    int video = findEntry("Video");
    int glass = findOp(Op::Kind::PushGlass);
    int slot = findOp(Op::Kind::Slot);
    int ring = findOp(Op::Kind::Stroke);
    assert(video >= 0 && glass >= 0 && slot >= 0 && ring >= 0);
    // Tree order: screen dialog glass, then sibling video plate, then
    // the fullscreen modal glass over everything, seat, ring. The
    // overlapping pair (video under modal glass) is correctly ordered;
    // dialog-vs-video never overlap (re-bucketing stays parked).
    int glass2 = findOp(Op::Kind::PushGlass, 1);
    assert(
        glass < video && video < glass2 && glass2 < slot && slot < ring
    );
    assert(findOp(Op::Kind::PushGlass, 2) < 0);  // two glass groups
    assert(rec.ops[static_cast<std::size_t>(glass)].clear);
    assert(rec.ops[static_cast<std::size_t>(glass2)].clear);
    assert(
        rec.ops[static_cast<std::size_t>(slot)].slot == kShowcaseSeatId
    );
    assert(findEntry("Video", 1) < 0);
    assert(findEntry("Card") >= 0 && findEntry("Button") >= 0);

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
      paintShowcaseInto(screen, over, dbox, sink, feed);
    }
    ctx.finish();
    const glim::paint::Scene& scene = ctx.scene();
    int glassGroups = 0;
    bool glassClear = true;
    bool slotInGlass = false;
    bool blitInGlass = false;
    int glyphRuns = 0;
    std::vector<std::string> rootSeq;
    std::function<void(const glim::paint::Group&, int, bool)>
        walkGroups = [&](const glim::paint::Group& g,
                         int depth,
                         bool inGlass) {
          glim::paint::visitGroup(
              g,
              [&](const glim::paint::Shape& s) {
                if (std::holds_alternative<glim::paint::Blit>(s)) {
                  rootSeq.push_back("Blit");
                  if (inGlass) blitInGlass = true;
                } else if (std::holds_alternative<
                               glim::paint::SlotHole>(s)) {
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
                if (cg) {
                  ++glassGroups;
                  glassClear = glassClear && c.params.glass &&
                               c.params.glass->variant ==
                                   glim::paint::GlassVariant::Clear;
                  rootSeq.push_back("Glass");
                }
                walkGroups(c, depth + 1, inGlass || cg);
              }
          );
        };
    walkGroups(scene.root, 0, false);
    assert(glassGroups == 2 && glassClear);
    assert(!slotInGlass && !blitInGlass);
    assert(glyphRuns > 0);
    auto pos = [&](const char* k) -> int {
      for (std::size_t i = 0; i < rootSeq.size(); ++i)
        if (rootSeq[i] == k) return static_cast<int>(i);
      return -1;
    };
    int rBlit = pos("Blit"), rSlot = pos("Slot"),
        rStroke = pos("Stroke");
    assert(rBlit >= 0 && rSlot >= 0 && rStroke >= 0);
    // Fullscreen modal glass rides over the video plate (the
    // overlapping under/over pair); the inline dialog glass sits in
    // tree order.
    int lastGlass = -1;
    for (int i = static_cast<int>(rootSeq.size()) - 1; i >= 0; --i)
      if (rootSeq[i] == "Glass") {
        lastGlass = i;
        break;
      }
    assert(lastGlass > rBlit && lastGlass < rSlot && rSlot < rStroke);
  }

  // Zero-codec peer: covered in the ops block above via Probe entries.
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

  std::cout << "showcase_test ok\n";
  return EXIT_SUCCESS;
}
