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
    assert(nodes[0].kind == "Tap" && nodes[0].id == "ok-btn" && nodes[0].focusable);
    assert(nodes[1].kind == "Lines" && nodes[1].label == "OK" && !nodes[1].focusable);
    assert(nodes[2].kind == "Seat" && nodes[2].id == "field" && nodes[2].focusable);
    assert(nodes[2].box.w == 200 && nodes[2].box.h == 40);
    assert(nodes[3].kind == "Lines" && nodes[3].label == "Name" && nodes[3].id.empty());
    assert(a11yFind(nodes, "field") && &a11yFind(nodes, "field")->get() == &nodes[2]);
    assert(!a11yFind(nodes, "nope"));
    std::ostringstream ad;
    dumpA11y(ad, nodes);
    assert(ad.str().find("Tap") != std::string::npos);
    assert(ad.str().find("*") != std::string::npos);
    assert(nodes[0].traits.role == A11yRole::Button && nodes[1].traits.role == A11yRole::Text);
    assert(nodes[2].traits.role == A11yRole::TextField);
    assert(nodes[0].traits.actions == A11yActions{A11yAction::Activate});
    assert(nodes[1].traits.actions.empty() && nodes[1].traits.states.empty());
  }
  {
    Block at = column(
      tap(
        lines("Wi-Fi"),
        [] {},
        semantic("wifi"),
        a11y(
          {.value = "Glim_5G",
           .states = {A11yState::Checkable, A11yState::Selected, A11yState::Disabled}}
        )
      ),
      lines("Saved", expand(), a11y({.role = A11yRole::Heading, .live = A11yLive::Assertive})),
      lines("", a11y({.live = A11yLive::Polite})),
      tap(
        lines("+"), [] {}, a11y({.actions = {A11yAction::Increment, A11yAction::Decrement}})
      )
    );
    Box ab = place(at, {0, 720, 0, 480});
    std::vector<A11yNode> nodes = a11yNodes(at, ab);
    assert(nodes.size() == 6);
    const A11yTraits& wifi = a11yFind(nodes, "wifi")->get().traits;
    assert(wifi.value == "Glim_5G" && !wifi.states.has(A11yState::Checked));
    assert(wifi.states.has(A11yState::Selected) && wifi.states.has(A11yState::Disabled));
    assert(wifi.actions == A11yActions{A11yAction::Activate});
    assert(at.get<Column>()->get().children[1].get<Custom>());
    assert(nodes[2].label == "Saved" && nodes[2].traits.live == A11yLive::Assertive);
    assert(nodes[3].label.empty() && nodes[3].traits.live == A11yLive::Polite);
    assert(nodes[4].traits.actions.has(A11yAction::Decrement));
    assert(!nodes[4].traits.actions.has(A11yAction::Activate));
    std::ostringstream ad;
    dumpA11y(ad, nodes);
    assert(
      ad.str() ==
      "button Tap [0,0 40x14] wifi * value=Glim_5G states=checkable,selected,disabled "
      "actions=activate\n"
      "text Lines [0,0 40x14] Wi-Fi\n"
      "heading Lines [0,14 40x438] Saved live=assertive\n"
      "text Lines [0,452 0x14] live=polite\n"
      "button Tap [0,466 8x14] * actions=increment,decrement\n"
      "text Lines [0,466 8x14] +\n"
    );
    A11yTraits base{.value = "a", .range = A11yRange{2, 0, 4}, .states = {A11yState::Checked}};
    A11yTraits top =
      overlay(base, {.states = {A11yState::Selected}, .actions = {A11yAction::Activate}});
    assert(top.value == "a" && top.range->now == 2);
    assert(top.states == (A11yStates{A11yState::Checked, A11yState::Selected}));
    assert(top.states.with(A11yState::Checked, false) == A11yStates{A11yState::Selected});
    assert(top.actions == A11yActions{A11yAction::Activate});
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
