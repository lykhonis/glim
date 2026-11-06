#include "test_helpers.h"

using namespace glim::toolkit;

void test_state() {
  {
    Store rs;
    assert(rs.revision() == 0);
    State<int> a = rs.make<int>(1);
    State<int> alias = a;
    assert(rs.revision() == 0);
    a = 2;
    assert(rs.revision() == 1 && *alias == 2);
    alias.set(3);
    assert(rs.revision() == 2 && *a == 3);
    State<std::string> s = rs.make<std::string>("x");
    s.set(std::string("y"));
    assert(rs.revision() == 3);
    s = "z";
    assert(rs.revision() == 4);
    Store other;
    assert(other.revision() == 0);
  }

  {
    Store gs;
    DirtyGate gate;
    assert(gate.check(gs.revision()));
    gate.acknowledge(gs.revision());
    assert(!gate.check(gs.revision()));
    State<int> gv = gs.make<int>(0);
    gv = 1;
    assert(gate.check(gs.revision()));
    gate.acknowledge(gs.revision());
    assert(!gate.check(gs.revision()));
    gate.invalidate();
    assert(gate.check(gs.revision()));
    gate.acknowledge(gs.revision());
    assert(!gate.check(gs.revision()));
  }

  {
    Store hs;
    State<int> picked = hs.make<int>(-1);
    Block st = row(gap(0), tap(lines("a"), [picked] { picked = 0; }), tap(lines("b"), [picked] {
                     picked = 1;
                   }));
    Box sbox = place(st, {0, 720, 0, 480});
    Screen screen;
    assert(screen.focus == -1 && !screen.focusVisible);
    assert(screen.needsPaint(hs));
    screen.painted(hs);
    assert(!screen.needsPaint(hs));
    assert(screen.pointerDown(st, sbox, 1, 1, 7));
    assert(screen.focus == 0 && screen.drag == 0 && screen.dragId == 7);
    assert(!screen.focusVisible);
    assert(*picked == 0);
    assert(!screen.pointerMove(st, sbox, 500, 400, true, 7));
    assert(screen.drag == 0 && *picked == 0);
    assert(!screen.pointerMove(st, sbox, 500, 400, true, 8));
    assert(screen.drag == 0);
    assert(!screen.pointerMove(st, sbox, 1, 1, false, 7));
    screen.pointerUp(st, sbox, 500, 400);
    assert(screen.drag == -1);
    assert(screen.needsPaint(hs));
    screen.painted(hs);
    assert(!screen.pointerDown(st, sbox, 500, 400, 9));
    assert(screen.focus == 0 && screen.drag == -1);
    assert(screen.key(st, sbox, Key::Next));
    assert(screen.focus == 1 && screen.focusVisible);
    assert(!screen.needsPaint(hs));
    picked = 1;
    assert(screen.needsPaint(hs));
    screen.painted(hs);
    assert(!screen.needsPaint(hs));
  }
}
