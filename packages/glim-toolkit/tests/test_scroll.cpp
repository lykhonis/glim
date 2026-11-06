#include "test_helpers.h"

using namespace glim::toolkit;

void test_scroll() {
  Store store;
  {
    auto filler = [](int i) { return lines("filler" + std::to_string(i) + "-filler-filler"); };
    Screen scr;
    Engine e;
    State<float> outer = store.make<float>(0.0f);
    Block tree = roll(vertical(), scrollOffset(outer), list(12, filler));
    // 12 x 14px = 168 content; viewport 100 → max 68.
    Box tb = place(tree, {0, 720, 0, 100});
    assert(scr.wheel(tree, tb, 4, 50, 0.0f, 30.0f, e));
    assert(*outer == 30.0f);
    assert(scr.wheel(tree, tb, 4, 50, 0.0f, 100.0f, e));
    assert(*outer == 68.0f);
    assert(!scr.wheel(tree, tb, 4, 50, 0.0f, 10.0f, e));
    assert(scr.wheel(tree, tb, 4, 50, 0.0f, -20.0f, e));
    assert(*outer == 48.0f);
    assert(!scr.wheel(tree, tb, 4, 50, 0.0f, 0.0f, e));
    assert(!scr.wheel(tree, tb, 4, 150, 0.0f, 10.0f, e));
    assert(*outer == 48.0f);
    Block bare = roll(vertical(), list(12, filler));
    Box bb = place(bare, {0, 720, 0, 100});
    assert(!scr.wheel(bare, bb, 4, 50, 0.0f, 10.0f, e));
    State<float> hout = store.make<float>(0.0f);
    State<float> vout = store.make<float>(0.0f);
    Block nested = roll(
        vertical(),
        scrollOffset(vout),
        roll(
            horizontal(),
            scrollOffset(hout),
            lines("12345678901234567890"),
            lines("12345678901234567890")
        ),
        list(12, filler)
    );
    Box nb = place(nested, {0, 120, 0, 60});
    assert(scr.wheel(nested, nb, 10, 10, 25.0f, 0.0f, e));
    assert(*hout == 25.0f && *vout == 0.0f);
    assert(scr.wheel(nested, nb, 10, 10, 0.0f, 12.0f, e));
    assert(*vout == 12.0f);
  }
}
