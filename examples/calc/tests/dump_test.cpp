#include <glim/paint/Context.h>
#include <glim/paint/Scene.h>

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
#include "paint/Demo.h"
#include "paint/Ops.h"
#include "paint/VideoFeed.h"

using namespace glim::toolkit;
using namespace glim::toolkit::paint;

std::string calcDump();
std::string modalDump();
std::string listDump();
std::string focusDump();
Block calcTree(State<std::string> out);
Block modalTree(Action onNo, Action onYes);

namespace {

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

}  // namespace

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : "examples/calc/golden";
  if (int rc = checkGolden(dir, "dump.txt", calcDump())) return rc;
  if (int rc = checkGolden(dir, "modal.txt", modalDump())) return rc;
  if (int rc = checkGolden(dir, "focus.txt", focusDump())) return rc;
  if (int rc = checkGolden(dir, "list.txt", listDump())) return rc;

  // Calc dispatch + focus end to end on real buttons (flat rows, direct
  // leaves).
  Store store;
  State<std::string> out = store.make<std::string>("12+3");
  Block tree = calcTree(out);
  Box box = place(tree, {0, 720, 0, 480});
  auto focusableList = focusables(tree, box);
  assert(focusableList.size() == 12);
  // First focusable is "7"; firing it appends through the cell.
  Box first = focusBox(tree, box, 0);
  assert(dispatchPointer(
      tree,
      box,
      first.x + 1,
      first.y + 1,
      Event::borrow(PointerDown{first.x + 1, first.y + 1})
  ));
  assert(*out == "12+37");
  // Spatial moves walk the digit grid.
  assert(moveFocus(tree, box, 0, FocusDir::Right) == 1);
  assert(moveFocus(tree, box, 1, FocusDir::Down) == 5);
  // Keys drive the same walk; Activate handles the focused digit.
  {
    int ki = deliverKey(tree, box, 0, Key::Right);
    assert(ki == 1);
    std::string before = *out;
    ki = deliverKey(tree, box, ki, Key::Activate);
    assert(ki == 1 && *out == before + "8");
  }
  // Ring overlay rides last on the calc record.
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

  // Modal overlay: dim fills the box, card centers via fills, paint
  // under→over. Buttons composed inside the Card stay reachable through
  // eachChild.
  State<int> answer = store.make<int>(0);
  Block modal =
      modalTree([answer] { answer = 1; }, [answer] { answer = 2; });
  Box mbox = place(modal, {0, 720, 0, 480});
  const Stack* st2 = modal.get<Stack>();
  assert(st2 && st2->children.size() == 2);
  auto modalFocus = focusables(modal, mbox);
  assert(modalFocus.size() == 2);
  // Hit the Yes button through the Card and handle it.
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
  assert(*answer == 2);
  // Focus walks from No to Yes.
  assert(moveFocus(modal, mbox, 0, FocusDir::Right) == 1);
  // Dim corner hits the dim, which ignores the event.
  const Block* dimHit = hit(modal, mbox, 10, 10);
  assert(
      dimHit && !handle(*dimHit, Event::borrow(PointerDown{10, 10}))
  );
  // Card padding itself resolves to the Card, which ignores the event.
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

  // List scroll: content taller than the viewport spills past it (no
  // clip).
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

  // T4 bridge: UiOp order is Blit < glass < slot < ring; the archive
  // round-trips byte-identically (and rejects truncation); the Scene
  // has the Blit before glass, exactly one Clear glass group, and the
  // SlotHole as a root sibling — never inside glass.
  {
    Block demo = paintDemoTree([] {}, [] {});
    Box dbox = place(demo, {0, 720, 0, 480});

    OpsRecorder rec;
    VideoFeed stubFeed;
    stubFeed.id = 7;
    stubFeed.foreign = true;
    {
      Engine e;
      paintTree(demo, dbox, e, rec, stubFeed);
      rec.slot(demoSeatBox(), kDemoSeatId);
      paintFocusRing(demo, dbox, 0, rec);
    }
    auto findOp = [&](UiOp::Kind k, int nth = 0) -> int {
      int seen = 0;
      for (std::size_t i = 0; i < rec.ops.size(); ++i)
        if (rec.ops[i].kind == k && seen++ == nth)
          return static_cast<int>(i);
      return -1;
    };
    int blit = findOp(UiOp::Kind::Blit);
    int glass = findOp(UiOp::Kind::PushGlass);
    int slot = findOp(UiOp::Kind::Slot);
    int ring = findOp(UiOp::Kind::Stroke);
    assert(blit >= 0 && glass >= 0 && slot >= 0 && ring >= 0);
    assert(blit < glass && glass < slot && slot < ring);
    assert(findOp(UiOp::Kind::PushGlass, 1) < 0);  // one glass group
    assert(rec.ops[static_cast<std::size_t>(glass)].clear);
    assert(rec.ops[static_cast<std::size_t>(slot)].slot == kDemoSeatId);
    assert(findOp(UiOp::Kind::Blit, 1) < 0);  // one video plate

    std::vector<std::uint8_t> bytes;
    encodeOps(rec.ops, bytes);
    std::vector<UiOp> back;
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
      paintDemoInto(demo, dbox, sink, feed);
    }
    ctx.finish();
    const glim::paint::Scene& scene = ctx.scene();
    int glassGroups = 0;
    bool glassClear = false;
    bool slotInGlass = false;
    bool blitInGlass = false;
    int glyphRuns = 0;
    std::vector<std::string> rootSeq;
    std::function<void(const glim::paint::Group&, int, bool)> walk =
        [&](const glim::paint::Group& g, int depth, bool inGlass) {
          glim::paint::visitGroup(
              g,
              [&](const glim::paint::Shape& s) {
                if (std::holds_alternative<glim::paint::Blit>(s)) {
                  if (depth == 0) rootSeq.push_back("Blit");
                  if (inGlass) blitInGlass = true;
                } else if (std::holds_alternative<
                               glim::paint::SlotHole>(s)) {
                  if (depth == 0) {
                    rootSeq.push_back("Slot");
                    const auto& hole =
                        std::get<glim::paint::SlotHole>(s);
                    assert(hole.id == kDemoSeatId);
                  }
                  if (inGlass) slotInGlass = true;
                } else if (std::holds_alternative<glim::paint::Stroke>(s
                           )) {
                  if (depth == 0) rootSeq.push_back("Stroke");
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
                  if (c.params.glass &&
                      c.params.glass->variant ==
                          glim::paint::GlassVariant::Clear)
                    glassClear = true;
                  if (depth == 0) rootSeq.push_back("Glass");
                }
                walk(c, depth + 1, inGlass || cg);
              }
          );
        };
    walk(scene.root, 0, false);
    assert(glassGroups == 1 && glassClear);
    assert(!slotInGlass && !blitInGlass);
    assert(glyphRuns > 0);  // button labels + dialog text painted
    auto pos = [&](const char* k) -> int {
      for (std::size_t i = 0; i < rootSeq.size(); ++i)
        if (rootSeq[i] == k) return static_cast<int>(i);
      return -1;
    };
    int rBlit = pos("Blit"), rGlass = pos("Glass"), rSlot = pos("Slot"),
        rStroke = pos("Stroke");
    assert(rBlit >= 0 && rGlass >= 0 && rSlot >= 0 && rStroke >= 0);
    assert(rBlit < rGlass && rGlass < rSlot && rSlot < rStroke);
  }

  std::cout << "dump_test ok\n";
  return EXIT_SUCCESS;
}
