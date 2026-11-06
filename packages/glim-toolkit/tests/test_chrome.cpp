#include "test_helpers.h"

using namespace glim::toolkit;

void test_chrome() {
  Block fc = column(gap(0), lines("a", expand()), lines("b", expand()));
  Box fcb = place(fc, {0, 720, 0, 100});
  assert(fcb.h == 100);
  {
    auto cp = fc.get<Column>();
    assert(cp && cp->get().children.size() == 2);
    Engine e;
    for (std::size_t i = 0; i < 2; ++i) {
      assert(cp->get().children[i].has<ExpandTag>());
      assert(cp->get().childBox(i, fcb, e).h == 50);
    }
  }

  Block st = tap(lines("x"), [] {}, semantic("ok"));
  {
    Rec recr;
    record(st, place(st, {0, 720, 0, 480}), recr);
    assert(recr.entries[0].kind == Tap::kKind);
    assert(recr.entries[0].mark == "ok");
  }

  Block p = tap(lines("ab"), [] {}, pad(6, 2));
  auto pp = p.get<Tap>();
  assert(pp && pp->get().padH == 6 && pp->get().padV == 2);
  Box pb = place(p, {0, 720, 0, 480});
  assert(pb.w == 16 + 12 && pb.h == 14 + 4);

  {
    Block pc = column(gap(0), pad(16), lines("ab"));
    Box pcb = place(pc, {0, 720, 0, 480});
    assert(pcb.w == 16 + 32 && pcb.h == 14 + 32);
    auto pcp = pc.get<Column>();
    assert(pcp);
    Engine e;
    Box cb0 = pcp->get().childBox(0, pcb, e);
    assert(cb0.x == 16 && cb0.y == 16);
    Block pr = row(gap(0), pad(10, 4), lines("ab"));
    Box prb = place(pr, {0, 720, 0, 480});
    assert(prb.w == 16 + 20 && prb.h == 14 + 8);
    auto prp = pr.get<Row>();
    assert(prp);
    Box rb0 = prp->get().childBox(0, prb, e);
    assert(rb0.x == 10 && rb0.y == 4);
  }
}
