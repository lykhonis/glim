#include "test_helpers.h"

using namespace glim::toolkit;

void test_tags() {
  Store store;
  {
    State<int> pc = store.make<int>(0);
    Block pinned = row(gap(0), tap(lines("a"), [pc] { pc = 1; }, Pin{}), lines("b"));
    auto rp = pinned.get<Row>();
    assert(rp && rp->get().children.size() == 2);
    assert(rp->get().children[0].has<Pin>());
    assert(rp->get().children[0].tag<Pin>());
    assert(!rp->get().children[1].has<Pin>());
    assert(handle(rp->get().children[0], Event::borrow(Activate{})));
    assert(*pc == 1);
  }

  {
    State<int> seen = store.make<int>(-99);
    Block tinted = column(Tint{3}, TintProbe{seen}, lines("x"));
    place(tinted, {0, 720, 0, 480});
    assert(*seen == 3);
    State<int> seen2 = store.make<int>(-99);
    Block nested = column(Tint{3}, column(Tint{7}, TintProbe{seen2}));
    place(nested, {0, 720, 0, 480});
    assert(*seen2 == 7);
    State<int> seen3 = store.make<int>(-99);
    Block plain = column(lines("y"), TintProbe{seen3});
    place(plain, {0, 720, 0, 480});
    assert(*seen3 == -1);
  }
  Block grid = column(
    gap(0),
    row(gap(0), tap(lines("1"), [] {}), tap(lines("2"), [] {}), tap(lines("3"), [] {})),
    row(gap(0), tap(lines("4"), [] {}), tap(lines("5"), [] {}), tap(lines("6"), [] {})),
    row(gap(0), tap(lines("7"), [] {}), tap(lines("8"), [] {}), tap(lines("9"), [] {}))
  );
  Box gridBox = place(grid, {0, 720, 0, 480});
  auto focusablesList = focusables(grid, gridBox);
  assert(focusablesList.size() == 9);
  int fi = 0;
  fi = moveFocus(grid, gridBox, fi, FocusDir::Right);
  assert(fi == 1);
  fi = moveFocus(grid, gridBox, fi, FocusDir::Down);
  assert(fi == 4);
  fi = moveFocus(grid, gridBox, fi, FocusDir::Left);
  assert(fi == 3);
  fi = moveFocus(grid, gridBox, fi, FocusDir::Up);
  assert(fi == 0);
  fi = moveFocus(grid, gridBox, fi, FocusDir::Prev);
  assert(fi == 0);
  fi = moveFocus(grid, gridBox, 8, FocusDir::Next);
  assert(fi == 8);
  Box fbox = focusBox(grid, gridBox, 4);
  assert(fbox.x == 8 && fbox.y == 14);

  {
    State<int> tapped = store.make<int>(0);
    Block keys = row(gap(0), tap(lines("a"), [tapped] { tapped = 1; }), tap(lines("b"), [tapped] {
                       tapped = 2;
                     }));
    Box kbox = place(keys, {0, 720, 0, 480});
    int ki = 0;
    ki = deliverKey(keys, kbox, ki, Key::Right);
    assert(ki == 1);
    ki = deliverKey(keys, kbox, ki, Key::Left);
    assert(ki == 0);
    ki = deliverKey(keys, kbox, ki, Key::Next);
    assert(ki == 1);
    ki = deliverKey(keys, kbox, ki, Key::Prev);
    assert(ki == 0);
    int before = *tapped;
    ki = deliverKey(keys, kbox, ki, Key::Activate);
    assert(ki == 0 && *tapped == before + 1);
    Block empty;
    Box ebox = place(empty, {0, 720, 0, 480});
    assert(deliverKey(empty, ebox, 0, Key::Right) == 0);
    assert(deliverKey(empty, ebox, 0, Key::Activate) == 0);
  }

  {
    State<int> tapped = store.make<int>(0);
    Block cap = row(gap(0), tap(lines("a"), [tapped] { tapped = 1; }), tap(lines("b"), [tapped] {
                      tapped = 2;
                    }));
    Box cbox = place(cap, {0, 720, 0, 480});
    int ai = focusAt(cap, cbox, 1, 1);
    assert(ai == 0);
    assert(focusAt(cap, cbox, 500, 400) == -1);
    assert(dispatchCaptured(cap, cbox, ai, Event::borrow(PointerMove{500, 400, true})));
    assert(*tapped == 0);
    assert(dispatchCaptured(cap, cbox, ai, Event::borrow(PointerDown{500, 400})));
    assert(*tapped == 1);
    assert(!dispatchCaptured(cap, cbox, 7, Event::borrow(PointerDown{1, 1})));
    assert(!dispatchCaptured(cap, cbox, -1, Event::borrow(PointerDown{1, 1})));
  }

  {
    Box ring = focusRingBox(grid, gridBox, 4);
    assert(ring.x == fbox.x - 2 && ring.y == fbox.y - 2);
    assert(ring.w == fbox.w + 4 && ring.h == fbox.h + 4);
    assert(focusRingBox(grid, gridBox, 99).w == 0);
  }
  // A tagged tap still claims the press over its label.
  {
    int taps = 0;
    for (Block b :
         {tap(
            lines("go"), [&taps] { ++taps; }, transform({})
          ),
          column(pad(10), tap(lines("go"), [&taps] { ++taps; }, translate(0, 0)))}) {
      Box bb = place(b, {0, 720, 0, 480});
      const Box tb = focusBox(b, bb, 0);
      const float x = tb.x + tb.w / 2, y = tb.y + tb.h / 2;
      assert(dispatchPointer(b, bb, x, y, Event::borrow(PointerDown{x, y})));
    }
    assert(taps == 2);
  }
  {
    State<int> tapped = store.make<int>(0);
    Block actionable = tap(lines("go"), [tapped] { tapped = 1; });
    Box abox = place(actionable, {0, 720, 0, 480});
    assert(dispatchPointer(actionable, abox, 1, 1, Event::borrow(PointerDown{1, 1})));
    assert(*tapped == 1);
    assert(!dispatchPointer(actionable, abox, 900, 900, Event::borrow(PointerDown{900, 900})));
    assert(!handle(actionable, Event::borrow(PointerMove{1, 1})));
  }

  // A tagged seat still reserves its hole: tags wrap it, and the wrapper paints through.
  {
    OpsRecorder rec;
    TestFeed feed;
    Block t = column(seat({.id = 9, .width = 200}, expand(), semantic("page")));
    Box tb = place(t, {0, 720, 0, 480});
    paintTree(t, tb, Engine{}, rec, feed);
    int slots = 0;
    for (const auto& o : rec.ops) {
      if (o.kind != Op::Kind::Slot) continue;
      assert(o.slot == 9 && o.box.w == 200 && o.box.h == 480);
      ++slots;
    }
    assert(slots == 1);
  }

  // Scope tags emit their push before the node paints and one pop after
  // its subtree, in a fixed order: shadow, effect, then the transform,
  // whose parts compose into one matrix.
  {
    OpsRecorder rec;
    TestFeed feed;
    const TransformTag moved{.translate = {3.f, -4.f}, .rotate = 30.f, .anchor = {0, 0}};
    Block t = column(
      shadow({.sigma = 12.f, .offsetX = 1.f, .offsetY = 4.f, .expandPx = 2.f, .opacity = 0.5f}),
      effectGlow({.radius = 5, .color = 0xff6a00ffu, .intensity = 0.3f}),
      transform(moved),
      lines("x")
    );
    Box tb = place(t, {0, 720, 0, 480});
    paintTree(t, tb, Engine{}, rec, feed);
    std::vector<Op::Kind> pushes;
    int pops = 0;
    for (const auto& o : rec.ops) {
      if (o.kind == Op::Kind::Pop) ++pops;
      if (o.kind == Op::Kind::PushShadow) {
        assert(o.sigma == 12.f && o.sox == 1.f && o.soy == 4.f && o.shadowColor == 0x000000a6u);
        assert(o.expand == 2.f && o.casterOpacity == 0.5f);
      }
      if (o.kind == Op::Kind::PushEffect) {
        assert(o.effect == static_cast<std::uint32_t>(EffectKind::Glow));
        assert(o.rgba == 0xff6a00ffu && o.e0 == 0.3f && o.radius == 5);
      }
      if (o.kind == Op::Kind::PushTransform) {
        // The corner (0, 0) of the box turns in place, then moves by (3, -4).
        const glim::Vec3 q = glim::projectPlane(o.matrix, {tb.x, tb.y});
        assert(std::fabs(q.x - (tb.x + 3)) < 1e-4f && std::fabs(q.y - (tb.y - 4)) < 1e-4f);
        const glim::Mat4 want = transformMatrix(moved, tb);
        for (int i = 0; i < 16; ++i) assert(o.matrix.m[i] == want.m[i]);
      }
      if (o.kind == Op::Kind::PushShadow || o.kind == Op::Kind::PushEffect ||
          o.kind == Op::Kind::PushTransform)
        pushes.push_back(o.kind);
    }
    assert(
      (pushes ==
       std::vector<Op::Kind>{Op::Kind::PushShadow, Op::Kind::PushEffect, Op::Kind::PushTransform})
    );
    assert(pops == 3);
  }

  // A fade ramps alpha from its edge inward in the node's own space, inside
  // its transform; an inert copy paints but takes no pointer, focus or a11y.
  {
    int taps = 0;
    const auto page = [&taps] { return tap(lines("page"), [&taps] { ++taps; }, pad(40, 20)); };
    Block tree = stack(
      page(),
      column(
        transform({.scale = {1, -1}, .anchor = {0.5f, 1}}),
        fade({.edge = FadeEdge::Bottom, .length = 30, .fromAlpha = 0.4f, .toAlpha = 0}),
        inert(),
        page()
      )
    );
    Box tb = place(tree, {0, 720, 0, 480});
    OpsRecorder rec;
    TestFeed feed;
    paintTree(tree, tb, Engine{}, rec, feed);
    int fades = 0, transforms = 0;
    for (const auto& o : rec.ops) {
      transforms += o.kind == Op::Kind::PushTransform;
      if (o.kind != Op::Kind::PushFade) continue;
      ++fades;
      // Transform first, then the fade inside it.
      assert(transforms == 1);
      const float bottom = tb.y + focusBox(tree, tb, 0).h;
      assert(o.fade.from.y == bottom && o.fade.to.y == bottom - 30);
      assert(o.fade.fromAlpha == 0.4f && o.fade.toAlpha == 0);
    }
    assert(fades == 1);
    // One focusable and one a11y button: the copy is inert.
    assert(focusables(tree, tb, Engine{}).size() == 1);
    int buttons = 0;
    for (const auto& n : a11yNodes(tree, tb)) buttons += n.focusable;
    assert(buttons == 1);
    // A press on the mirror below the page reaches nothing.
    const Box pb = focusBox(tree, tb, 0);
    const float below = pb.y + pb.h + 4;
    assert(!dispatchPointer(tree, tb, pb.x + 4, below, Event::borrow(PointerDown{pb.x + 4, below}))
    );
    assert(
      dispatchPointer(tree, tb, pb.x + 4, pb.y + 4, Event::borrow(PointerDown{pb.x + 4, pb.y + 4}))
    );
    assert(taps == 1);
  }

  // Each edge ramps inward from its own side; length 0 spans the node.
  {
    const Box b{10, 20, 100, 40};
    const glim::Fade top = fadeIn({.edge = FadeEdge::Top}, b);
    assert(top.from.y == 20 && top.to.y == 60 && top.from.x == 60);
    const glim::Fade left = fadeIn({.edge = FadeEdge::Left, .length = 25}, b);
    assert(left.from.x == 10 && left.to.x == 35 && left.from.y == 40);
    const glim::Fade right = fadeIn({.edge = FadeEdge::Right}, b);
    assert(right.from.x == 110 && right.to.x == 10);
  }

  // Skew and a mirror are transforms too: hit testing follows them.
  {
    int taps = 0;
    Block skewed = column(
      transform({.skew = {20, 0}, .anchor = {0, 1}}),
      tap(
        lines("go"), [&taps] { ++taps; }, pad(40, 20)
      )
    );
    Box sb = place(skewed, {0, 720, 0, 480});
    const Box tb = focusBox(skewed, sb, 0);
    // As CSS skewX, about the bottom edge the top leans left: the top-right
    // corner is uncovered and the space left of the top-left is covered.
    const float lean = tb.h * std::tan(20.f * 3.14159265f / 180.f);
    const float rx = tb.x + tb.w - 2, lx = tb.x - lean + 2, ty = tb.y + 2;
    assert(!dispatchPointer(skewed, sb, rx, ty, Event::borrow(PointerDown{rx, ty})));
    assert(dispatchPointer(skewed, sb, lx, ty, Event::borrow(PointerDown{lx, ty})));
    Block mirrored = column(
      transform({.scale = {1, -1}, .anchor = {0.5f, 1}}),
      tap(
        lines("go"), [&taps] { ++taps; }, pad(40, 20)
      )
    );
    Box mb = place(mirrored, {0, 720, 0, 480});
    const Box fb = focusBox(mirrored, mb, 0);
    // Flipped about its bottom edge, the tap sits just below where it was.
    const float below = fb.y + fb.h + 4;
    assert(
      dispatchPointer(mirrored, mb, fb.x + 4, below, Event::borrow(PointerDown{fb.x + 4, below}))
    );
    assert(taps == 2);
  }
}

void test_tilt() {
  // Like a rotation, a tilt moves a node's children (the node paints first):
  // a column tilted 45 degrees about Y around its center, holding a
  // 200x100 tap (32x14 label padded). Pointers hit what paint drew.
  int taps = 0;
  Block plate = tap(lines("page"), [&taps] { ++taps; }, pad(84, 43));
  Block tree = column(pad(100), transform({.tilt = {0, 45}, .perspective = 400}), std::move(plate));
  Box tb = place(tree, {0, 400, 0, 300});
  const Box pb = focusBox(tree, tb, 0);
  assert(pb.w == 200 && pb.h == 100);
  const float cx = pb.x + pb.w / 2, cy = pb.y + pb.h / 2;

  OpsRecorder rec;
  TestFeed feed;
  paintTree(tree, tb, Engine{}, rec, feed);
  int tilts = 0, pops = 0;
  for (const auto& o : rec.ops) {
    if (o.kind == Op::Kind::PushTransform) {
      const glim::Vec3 pivot = glim::projectPlane(o.matrix, {cx, cy});
      assert(o.matrix.is3D() && std::fabs(pivot.x / pivot.z - cx) < 1e-3f);
      ++tilts;
    }
    pops += o.kind == Op::Kind::Pop;
  }
  assert(tilts == 1 && pops >= 1);

  // The pivot projects onto itself; the far (right) edge pulls in, so a
  // point inside the flat box near that edge now misses.
  assert(dispatchPointer(tree, tb, cx, cy, Event::borrow(PointerDown{cx, cy})));
  assert(taps == 1);
  const float farX = pb.x + pb.w - 4;
  assert(!dispatchPointer(tree, tb, farX, cy, Event::borrow(PointerDown{farX, cy})));
  assert(taps == 1);
  // Near (left) edge grows: just above the flat top, by the left edge, hits.
  const glim::Mat4 m = transformMatrix({.tilt = {0, 45}, .perspective = 400}, pb);
  const glim::Vec3 corner = glim::projectPlane(m, {pb.x + 4, pb.y + 4});
  const float nx = corner.x / corner.z, ny = corner.y / corner.z;
  assert(ny < pb.y);
  assert(dispatchPointer(tree, tb, nx, ny + 1, Event::borrow(PointerDown{nx, ny + 1})));
  assert(taps == 2);

  // fit keeps the projected quad inside the node's box, so it never covers
  // neighbors; without it the near edge reaches outside.
  for (const float deg : {30.f, 60.f, -75.f}) {
    const Box b{100, 100, 400, 300};
    const auto bounds = [&](const glim::Mat4& mt) {
      float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
      for (const glim::Vec2 c :
           {glim::Vec2{100, 100}, glim::Vec2{500, 100}, glim::Vec2{100, 400}, glim::Vec2{500, 400}
           }) {
        const glim::Vec3 q = glim::projectPlane(mt, c);
        x0 = std::min(x0, q.x / q.z), y0 = std::min(y0, q.y / q.z);
        x1 = std::max(x1, q.x / q.z), y1 = std::max(y1, q.y / q.z);
      }
      return Box{x0, y0, x1 - x0, y1 - y0};
    };
    const Box loose = bounds(transformMatrix({.tilt = {0, deg}, .perspective = 600}, b));
    assert(loose.y < b.y - 1);
    const Box fitted =
      bounds(transformMatrix({.tilt = {0, deg}, .perspective = 600, .fit = true}, b));
    assert(fitted.x >= b.x - 0.5f && fitted.y >= b.y - 0.5f);
    assert(fitted.x + fitted.w <= b.x + b.w + 0.5f && fitted.y + fitted.h <= b.y + b.h + 0.5f);
    assert(fitted.h > b.h * 0.6f);
  }

  // Paint gets the same fit scale hit testing uses.
  {
    const TransformTag fitTilt{.tilt = {0, 60}, .perspective = 600, .fit = true};
    Block fitted = column(pad(50), transform(fitTilt), tap(lines("page"), [] {}, pad(100, 80)));
    Box fb = place(fitted, {0, 600, 0, 400});
    OpsRecorder r;
    TestFeed f;
    paintTree(fitted, fb, Engine{}, r, f);
    int seen = 0;
    for (const auto& o : r.ops) {
      if (o.kind != Op::Kind::PushTransform) continue;
      const glim::Mat4 want = transformMatrix(fitTilt, fb);
      for (int i = 0; i < 16; ++i) assert(o.matrix.m[i] == want.m[i]);
      ++seen;
    }
    assert(seen == 1);
  }

  // Focus and a11y report the node in its own space; a transform that
  // changes nothing adds no group.
  assert(focusAt(tree, tb, cx, cy) == 0);
  Block flat = column(transform({}), tap(lines("page"), [] {}));
  OpsRecorder none;
  Box fb = place(flat, {0, 400, 0, 300});
  paintTree(flat, fb, Engine{}, none, feed);
  for (const auto& o : none.ops) assert(o.kind != Op::Kind::PushTransform);
}
