#include "test_helpers.h"

using namespace glim::toolkit;

void test_layout() {
  Store store;
  // share = (720 - 3*8) / 4 = 174.
  Block fr = row(
    gap(8), lines("a", expand()), lines("b", expand()), lines("c", expand()), lines("d", expand())
  );
  Box fb = place(fr, {0, 720, 0, 480});
  assert(fb.w == 720);
  {
    auto rp = fr.get<Row>();
    assert(rp && rp->get().children.size() == 4);
    Engine e;
    for (std::size_t i = 0; i < 4; ++i) {
      assert(rp->get().children[i].has<ExpandTag>());
      assert(rp->get().childBox(i, fb, e).w == 174);
    }
    Rec recr;
    record(fr, fb, recr);
    for (const auto& en : recr.entries) assert(en.kind != "Fill");
  }

  Block ov = row(gap(0), lines("12345678901234567890"), lines("12345678901234567890"));
  Box ovb = place(ov, {0, 100, 0, 480});
  assert(ovb.w == 100);
  {
    Rec recr;
    record(ov, ovb, recr);
    assert(recr.entries[1].box.w == 160 && recr.entries[2].box.w == 160);
  }

  Block modal = stack(lines("under"), lines("over"));
  Box modalBox = place(modal, {0, 720, 0, 480});
  {
    Rec recr;
    record(modal, modalBox, recr);
    assert(recr.entries.size() == 3);
    assert(recr.entries[1].text == "under" && recr.entries[2].text == "over");
    assert(recr.entries[1].box.x == recr.entries[2].box.x);
    assert(recr.entries[1].box.y == recr.entries[2].box.y);
    BlockRef top = hit(modal, modalBox, 2, 2);
    assert(top && top->get().get<Lines>() && top->get().get<Lines>()->get().text == "over");
    BlockRef miss = hit(modal, modalBox, 500, 400);
    assert(!miss);
  }

  Block wide = roll(horizontal(), lines("12345678901234567890"), lines("12345678901234567890"));
  Box wideBox = place(wide, {0, 100, 0, 480});
  assert(wideBox.w == 100);
  {
    Rec recr;
    record(wide, wideBox, recr);
    assert(recr.entries[1].box.w == 160 && recr.entries[2].box.w == 160);
    assert(recr.entries[2].box.x == 160);
  }
  State<float> off2 = store.make<float>(160.0f);
  Block scrolled = roll(
    horizontal(), scrollOffset(off2), lines("12345678901234567890"), lines("12345678901234567890")
  );
  Box scrolledBox = place(scrolled, {0, 100, 0, 480});
  {
    Rec recr;
    record(scrolled, scrolledBox, recr);
    assert(recr.entries[1].box.x == -160);
    BlockRef top = hit(scrolled, scrolledBox, 2, 2);
    assert(
      top && top->get().get<Lines>() &&
      top->get().get<Lines>()->get().text == "12345678901234567890"
    );
  }

  // Partially visible controls still hit.
  {
    auto filler = [](int i) { return lines("filler" + std::to_string(i) + "-filler-filler"); };
    State<float> zero = store.make<float>(0.0f);
    Block tall = roll(
      vertical(),
      scrollOffset(zero),
      tap(lines("top"), [] {}),
      list(10, filler),
      tap(lines("buried"), [] {})
    );
    // top [0,14), fillers [14,154), buried [154,168); viewport 100.
    Box tallBox = place(tall, {0, 720, 0, 100});
    assert(tallBox.h == 100);
    BlockRef vis = hit(tall, tallBox, 4, 4);
    assert(vis && vis->get().get<Tap>());
    assert(!hit(tall, tallBox, 4, 120));
    assert(!hit(tall, tallBox, 4, 160));
    // Scrolled by 60: filler 0 ([-46,-32)) misses, filler 4 ([10,24)) hits at y=20.
    State<float> down = store.make<float>(60.0f);
    Block moved = roll(vertical(), scrollOffset(down), tap(lines("top"), [] {}), list(10, filler));
    Box movedBox = place(moved, {0, 720, 0, 100});
    assert(!hit(moved, movedBox, 4, -40));
    BlockRef part = hit(moved, movedBox, 4, 20);
    assert(part && part->get().get<Lines>());
  }
}
