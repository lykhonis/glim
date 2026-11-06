#include "test_helpers.h"

using namespace glim::toolkit;

void test_motion() {
  Store store;
  {
    assert(easeAt(Ease::Linear, 0.25f) == 0.25f);
    assert(easeAt(Ease::Out, 0.0f) == 0.0f && easeAt(Ease::Out, 1.0f) == 1.0f);
    assert(easeAt(Ease::Out, 0.5f) == 1.0f - 0.5f * 0.5f * 0.5f);
    assert(easeAt(Ease::InOut, 0.5f) == 0.5f);
    Tween t{0.0f, 100.0f, 1.0f, 2.0f, Ease::Linear};
    assert(t.at(0.0f) == 0.0f && t.at(1.0f) == 0.0f);
    assert(t.at(2.0f) == 50.0f && t.at(3.0f) == 100.0f && t.at(9.0f) == 100.0f);
    assert(!t.done(2.0f) && t.done(3.0f));
    Tween snap{5.0f, 9.0f, 0.0f, 0.0f};
    assert(snap.at(0.0f) == 9.0f && snap.done(0.0f));
    Tween f = flingTo(10.0f, 200.0f, 0.0f, 68.0f, 0.0f);
    assert(f.from == 10.0f && f.to == 40.0f && f.dur > 0.44f && f.dur < 0.46f);
    assert(f.at(0.225f) > 10.0f && f.at(0.225f) < 40.0f);
    assert(flingTo(60.0f, 200.0f, 0.0f, 68.0f, 0.0f).to == 68.0f);
    assert(flingTo(10.0f, 0.0f, 0.0f, 68.0f, 0.0f).to == 10.0f);
    Clock c;
    c.advance(0.016f);
    assert(c.now > 0.015f && c.now < 0.017f);
    c.advance(-1.0f);
    assert(c.now > 0.015f && c.now < 0.017f);
  }

  {
    auto filler = [](int i) { return lines("filler" + std::to_string(i) + "-filler-filler"); };
    Screen scr;
    Engine e;
    State<float> off = store.make<float>(0.0f);
    Block tree = roll(vertical(), scrollOffset(off), list(12, filler));
    // 12 x 14px = 168 content; viewport 100 → max 68.
    Box tb = place(tree, {0, 720, 0, 100});
    assert(scr.wheel(tree, tb, 4, 50, 0.0f, 30.0f, e));
    scr.clock.advance(1.0f / 60.0f);
    assert(scr.wheel(tree, tb, 4, 50, 0.0f, 30.0f, e));
    scr.clock.advance(1.0f / 60.0f);
    assert(scr.wheel(tree, tb, 4, 50, 0.0f, 30.0f, e));
    assert(*off == 68.0f);
    assert(!scr.moving());
    assert(!scr.tick(tree, tb, e));
    Screen slow;
    State<float> soff = store.make<float>(0.0f);
    Block stree = roll(vertical(), scrollOffset(soff), list(12, filler));
    Box stb = place(stree, {0, 720, 0, 100});
    assert(slow.wheel(stree, stb, 4, 50, 0.0f, 10.0f, e));
    slow.clock.advance(0.5f);
    assert(!slow.moving() && !slow.tick(stree, stb, e));
    assert(slow.wheel(stree, stb, 4, 50, 0.0f, 10.0f, e));
    assert(*soff == 20.0f && !slow.moving());
    Screen fast;
    State<float> foff = store.make<float>(0.0f);
    Block ftree = roll(vertical(), scrollOffset(foff), list(12, filler));
    Box ftb = place(ftree, {0, 720, 0, 100});
    assert(fast.wheel(ftree, ftb, 4, 50, 0.0f, 20.0f, e));
    fast.clock.advance(1.0f / 60.0f);
    assert(fast.wheel(ftree, ftb, 4, 50, 0.0f, 20.0f, e));
    assert(*foff == 40.0f && fast.moving());
    bool moved = false;
    for (int i = 0; i < 120 && fast.moving(); ++i) {
      fast.clock.advance(1.0f / 60.0f);
      moved = fast.tick(ftree, ftb, e) || moved;
    }
    assert(moved && !fast.moving());
    assert(*foff > 40.0f && *foff <= 68.0f);
    assert(!fast.tick(ftree, ftb, e));
  }
}
