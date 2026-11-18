#include <cassert>

#include "glim/toolkit.h"

using namespace glim::toolkit;

void test_slider() {
  Store store;
  Engine e;
  e.store = store;
  State<float> v = rootScope(e).state("tilt", 0.0f);
  const auto build = [&] {
    return column(pad(10), slider({.value = v, .lo = -45, .hi = 45, .step = 5, .width = 200}));
  };
  Block tree = build();
  Box tb = e.place(tree, {0, 400, 0, 100});
  const Box sb = focusBox(tree, tb, 0, e);
  assert(sb.w == 200 && sb.h > 0);
  const float cy = sb.y + sb.h / 2;

  // A press on the track jumps there, snapped to the step; a drag follows.
  Screen screen;
  screen.pointerDown(tree, tb, sb.x + sb.w * 0.75f, cy, 1, e);
  assert(*v == 20.0f || *v == 25.0f);
  tree = build();
  screen.pointerMove(tree, tb, sb.x + sb.w, cy, true, 1, e);
  assert(*v == 45.0f);
  tree = build();
  screen.pointerMove(tree, tb, sb.x - 50, cy, true, 1, e);
  assert(*v == -45.0f);
  tree = build();
  screen.pointerUp(tree, tb, sb.x - 50, cy, e);

  // Keys and screen readers step it, clamped to its range.
  const auto traits = [&] {
    tree = build();
    for (const A11yNode& n : a11yNodes(tree, tb, e)) {
      if (n.traits.role == A11yRole::Slider) return n.traits;
    }
    return A11yTraits{};
  };
  assert(traits().role == A11yRole::Slider);
  assert(traits().range && traits().range->now == -45.0f && traits().range->min == -45.0f);
  tree = build();
  assert(dispatchPointer(tree, tb, sb.x + 1, cy, Event::borrow(Adjust{1}), e));
  assert(*v == -40.0f);
  tree = build();
  assert(dispatchPointer(tree, tb, sb.x + 1, cy, Event::borrow(Adjust{-1}), e));
  tree = build();
  assert(dispatchPointer(tree, tb, sb.x + 1, cy, Event::borrow(Adjust{-1}), e));
  assert(*v == -45.0f);
  // Right steps up when focused.
  tree = build();
  screen.focus = 0;
  screen.key(tree, tb, Key::Right, e);
  assert(*v == -40.0f);
}
