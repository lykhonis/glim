#include "test_helpers.h"

using namespace glim::toolkit;

void test_basics() {
  Store store;
  Block b = tap(lines("Play"), [] {}, primary(), pad(4));
  auto bb = b.get<Tap>();
  assert(bb && bb->get().isPrimary && bb->get().padH == 4 && bb->get().padV == 4);
  assert(bb->get().children.size() == 1);
  auto bl = bb->get().children[0].get<Lines>();
  assert(bl && bl->get().text == "Play");
  assert(!handle(lines("x"), Event::borrow(Activate{})));
  assert(!handle(lines("x"), Event::borrow(PointerMove{1, 1})));

  Block s = stack(lines("under"), column(lines("over")));
  auto ss = s.get<Stack>();
  assert(ss && ss->get().children.size() == 2);
  Box sb = place(s, {0, 720, 0, 480});
  assert(sb.w > 0 && sb.h > 0);

  State<float> off = store.make<float>(0.0f);
  Block r = roll(horizontal(), scrollOffset(off), lines("a"), lines("b"));
  auto rr = r.get<Roll>();
  assert(rr && rr->get().axis == kAxisHorizontal && rr->get().scrollOff);

  State<int> n = store.make<int>(41);
  Counter counter{n};
  Block tree = counter.view();
  auto col = tree.get<Column>();
  assert(col && col->get().children.size() == 2);
  assert(handle(col->get().children[1], Event::borrow(Activate{})));
  assert(*n == 42);

  State<int> sel = store.make<int>(1);
  Block m = myRadial(sel, gap(8), radius(6), tap(lines("x"), [] {}));
  auto mm = m.get<Custom>()->get().as<Radial>();
  assert(mm && mm->get().gap == 8 && mm->get().radius == 6 && mm->get().children.size() == 2);
  Box mb = place(m, {0, 720, 0, 480});
  assert(mb.w > 0 && mb.h > 0);
  {
    Box fb3 = place(lines("MMM"), {0, 720, 0, 480});
    assert(fb3.w == 24 && fb3.h == 14);
  }
  assert(focusables(m, mb).size() == 1);

  {
    Block at = column(
        tap(
            lines("OK"), [] {}, semantic("ok-btn")
        ),
        seat({.id = 3, .width = 200.0f, .height = 40.0f}, semantic("field")),
        lines("Name")
    );
    Box ab = place(at, {0, 720, 0, 480});
    std::vector<A11yNode> nodes = a11yNodes(at, ab);
    assert(nodes.size() == 4);
    assert(nodes[0].role == "Tap" && nodes[0].id == "ok-btn" && nodes[0].focusable);
    assert(nodes[1].role == "Lines" && nodes[1].label == "OK" && !nodes[1].focusable);
    assert(nodes[2].role == "Seat" && nodes[2].id == "field" && nodes[2].focusable);
    assert(nodes[2].box.w == 200 && nodes[2].box.h == 40);
    assert(nodes[3].role == "Lines" && nodes[3].label == "Name" && nodes[3].id.empty());
    assert(a11yFind(nodes, "field") && &a11yFind(nodes, "field")->get() == &nodes[2]);
    assert(!a11yFind(nodes, "nope"));
    std::ostringstream ad;
    dumpA11y(ad, nodes);
    assert(ad.str().find("Tap") != std::string::npos);
    assert(ad.str().find("*") != std::string::npos);
  }
  Block f = seat({.id = 7, .width = 688, .height = 40}, semantic("field"));
  auto fs = f.get<Seat>();
  assert(fs && fs->get().id == 7 && fs->get().semantic == "field");
  Box fb2 = place(f, {0, 720, 0, 480});
  assert(fb2.w == 688 && fb2.h == 40);
  assert(focusables(f, fb2).size() == 1);
  assert(handle(f, Event::borrow(PointerDown{1, 1})));
  assert(handle(f, Event::borrow(Activate{})));
  assert(!handle(f, Event::borrow(PointerMove{1, 1})));
  {
    Box tb = focusBox(m, mb, 0);
    assert(
        dispatchPointer(m, mb, tb.x + 1, tb.y + 1, Event::borrow(PointerDown{tb.x + 1, tb.y + 1}))
    );
  }

  {
    Block ab = toBlock(AtBox{});
    Box abox = place(ab, {0, 720, 0, 480});
    assert(dispatchPointer(ab, abox, 11, 6, Event::borrow(PointerDown{11, 6})));
    auto ap = ab.get<Custom>()->get().as<AtBox>();
    assert(ap && ap->get().seen.x == abox.x && ap->get().seen.y == abox.y);
    assert(ap->get().seen.w == 50 && ap->get().seen.h == 20);
  }
}
