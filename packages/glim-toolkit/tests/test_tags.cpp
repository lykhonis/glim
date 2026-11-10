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
  {
    State<int> tapped = store.make<int>(0);
    Block actionable = tap(lines("go"), [tapped] { tapped = 1; });
    Box abox = place(actionable, {0, 720, 0, 480});
    assert(dispatchPointer(actionable, abox, 1, 1, Event::borrow(PointerDown{1, 1})));
    assert(*tapped == 1);
    assert(!dispatchPointer(actionable, abox, 900, 900, Event::borrow(PointerDown{900, 900})));
    assert(!handle(actionable, Event::borrow(PointerMove{1, 1})));
  }

  // Scope tags emit their push before the node paints and one pop after
  // its subtree, in a fixed order: shadow, effect, then transforms.
  {
    OpsRecorder rec;
    struct Feed {
      std::uint32_t id = 0;
      bool foreign = false;
    } feed;
    Block t = column(
        shadow(12.f, 1.f, 4.f, 0x000000a6u, 2.f, 0.5f),
        effectGlow(0xff6a00ffu, 0.3f, 5),
        translate(3.f, -4.f),
        rotate(30.f, 5.f, 6.f),
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
      if (o.kind == Op::Kind::PushTransform) assert(o.dx == 3.f && o.dy == -4.f);
      if (o.kind == Op::Kind::PushRotate)
        assert(o.angleDeg == 30.f && o.rcx == 5.f && o.rcy == 6.f);
      if (o.kind == Op::Kind::PushShadow || o.kind == Op::Kind::PushEffect ||
          o.kind == Op::Kind::PushTransform || o.kind == Op::Kind::PushRotate)
        pushes.push_back(o.kind);
    }
    assert(
        (pushes ==
         std::vector<Op::Kind>{
             Op::Kind::PushShadow,
             Op::Kind::PushEffect,
             Op::Kind::PushTransform,
             Op::Kind::PushRotate
         })
    );
    assert(pops == 4);
  }
}
