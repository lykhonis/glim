#include <cmath>

#include "test_helpers.h"

using namespace glim::toolkit;

namespace {

struct Seen {
  Box box{0, 0, 0, 0};
  float x = -1;
  float y = -1;
  int hits = 0;
};

// Records the box and the event point handleAt receives.
struct Probe {
  Seen* seen;
  const char* kind() const { return "Probe"; }
  Box place(Constraints c, const Engine&) const { return detail::resolveSize(50.0f, 20.0f, c); }
  bool handleAt(const Box& b, const Event& ev) const {
    auto d = ev.as<PointerDown>();
    if (!d) return false;
    *seen = {b, d->get().x, d->get().y, seen->hits + 1};
    return true;
  }
};

bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

bool press(const Block& tree, const Box& box, float x, float y) {
  return dispatchPointer(tree, box, x, y, Event::borrow(PointerDown{x, y}));
}

}  // namespace

void test_hit() {
  Store store;
  // Translate moves children: they hit where they paint, in their own space.
  {
    Seen seen;
    Block tree = column(translate(100, 50), toBlock(Probe{&seen}));
    Box tb = place(tree, {0, 720, 0, 480});
    assert(!press(tree, tb, 10, 5));
    assert(press(tree, tb, 110, 55));
    assert(seen.hits == 1 && near(seen.x, 10) && near(seen.y, 5));
    assert(seen.box.x == 0 && seen.box.y == 0);
    assert(focusAt(tree, tb, 110, 55) == 0);
    assert(focusAt(tree, tb, 10, 5) == -1);
    assert(dispatchCaptured(tree, tb, 0, Event::borrow(PointerDown{120, 60})));
    assert(near(seen.x, 20) && near(seen.y, 10));
  }
  // Rotate 90 degrees clockwise about (25, 10): local (30, 10) paints at (25, 15).
  {
    Seen seen;
    Block tree = column(rotate({.angleDeg = 90, .cx = 25, .cy = 10}), toBlock(Probe{&seen}));
    Box tb = place(tree, {0, 720, 0, 480});
    assert(press(tree, tb, 25, 15));
    assert(near(seen.x, 30) && near(seen.y, 10));
    assert(!press(tree, tb, 45, 10));
    assert(seen.hits == 1);
  }
  // Nested: the outer translate applies before the inner rotate.
  {
    Seen seen;
    Block tree = column(
      translate(200, 0), column(rotate({.angleDeg = 90, .cx = 25, .cy = 10}), toBlock(Probe{&seen}))
    );
    Box tb = place(tree, {0, 720, 0, 480});
    assert(press(tree, tb, 225, 15));
    assert(near(seen.x, 30) && near(seen.y, 10));
  }
  // Scroll is layout: the box moves, the point stays in window space.
  {
    Seen seen;
    State<float> off = store.make<float>(22.0f);
    Block tree = roll(
      vertical(), scrollOffset(off), lines("a"), lines("b"), lines("c"), toBlock(Probe{&seen})
    );
    Box tb = place(tree, {0, 720, 0, 40});
    assert(press(tree, tb, 5, 25));
    assert(seen.box.y == 20 && near(seen.x, 5) && near(seen.y, 25));
  }
  // Wheel finds a roll where its translated parent paints it.
  {
    Screen scr;
    State<float> off = store.make<float>(0.0f);
    auto filler = [](int i) { return lines("filler" + std::to_string(i)); };
    Block tree = column(translate(0, 200), roll(vertical(), scrollOffset(off), list(12, filler)));
    Box tb = place(tree, {0, 720, 0, 100});
    assert(!scr.wheel(tree, tb, 4, 300, 0, 10, Engine{}));
    assert(scr.wheel(tree, tb, 4, 250, 0, 10, Engine{}));
    assert(*off == 10.0f);
  }
}
