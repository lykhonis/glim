#include "glim/toolkit/Toolkit.h"

#include <cassert>
#include <iostream>

using namespace glim::toolkit;

static_assert(has_children<Row>::value, "");
static_assert(has_children<Column>::value, "");
static_assert(has_children<Stack>::value, "");
static_assert(has_children<List>::value, "");
static_assert(has_children<Roll>::value, "");
static_assert(has_children<Tap>::value, "");
// Member spelling: containers traverse without an eachChild method.
static_assert(has_child_vector<Row>::value, "");
static_assert(!has_eachChild<Row>::value, "");
static_assert(has_child_vector<Tap>::value && has_childBox<Tap>::value, "");
static_assert(!has_children<Empty>::value, "");
static_assert(!has_children<Lines>::value, "");
static_assert(!has_children<Seat>::value, "");
static_assert(!has_inner_box<Seat>::value, "");
static_assert(has_handle<Seat>::value, "");
static_assert(has_kind<Seat>::value, "");
static_assert(!has_record<Seat>::value, "");
static_assert(has_inner_box<Tap>::value, "");
static_assert(!has_inner_box<Row>::value, "");
static_assert(!has_handle_at<Tap>::value, "");
static_assert(!has_handle_at<Row>::value, "");

namespace glim {
namespace toolkit {

struct Radial : WithGap, WithRadius {
  const char* kind() const { return "Radial"; }
  std::vector<Block> children;
  Box place(Constraints c, const Engine& e) const {
    float w = 0, h = 0;
    for (const auto& child : children) {
      Box b = e.place(child, {0, c.maxW, 0, c.maxH});
      w = w > b.w ? w : b.w;
      h += b.h + static_cast<float>(gap);
    }
    if (!children.empty()) h -= static_cast<float>(gap);
    float d = w > h ? w : h;
    d += static_cast<float>(radius) * 2.0f;
    if (d < c.minW) d = c.minW;
    if (d > c.maxW) d = c.maxW;
    return {0, 0, d, d};
  }
  Box childBox(std::size_t i, const Box& b, const Engine& e) const {
    float y = b.y;
    for (std::size_t k = 0; k < children.size(); ++k) {
      Box kb = e.place(children[k], {0, b.w, 0, b.h});
      kb.x = b.x;
      kb.y = y;
      if (k == i) return kb;
      y += kb.h + static_cast<float>(gap);
    }
    return b;
  }
};

struct Pin {};

template <>
struct is_tag<Pin> : std::true_type {};

template <>
struct is_slot_tag<Pin> : std::true_type {};

struct Tint {
  int v = 0;
};

template <>
struct is_tag<Tint> : std::true_type {};

template <>
struct is_slot_tag<Tint> : std::true_type {};

struct TintProbe {
  static constexpr const char* kKind = "TintProbe";
  const char* kind() const { return kKind; }
  State<int> seen;
  Box place(Constraints c, const Engine& e) const {
    const Tint* t = e.get<Tint>();
    seen = t ? t->v : -1;
    return detail::resolveSize(10, 10, c);
  }
};

}  // namespace toolkit
}  // namespace glim

namespace {

struct Counter {
  glim::toolkit::State<int> count;
  glim::toolkit::Block view() const {
    auto c = count;
    using namespace glim::toolkit;
    return column(
        gap(8),
        lines("Count: " + std::to_string(*c)),
        tap(
            lines("Inc"), [c] { c = *c + 1; }, primary()
        )
    );
  }
};

template <class... As>
glim::toolkit::Block myRadial(glim::toolkit::State<int> sel, As&&... as) {
  using namespace glim::toolkit;
  Radial m;
  m.children.push_back(lines("sel=" + std::to_string(*sel)));
  configure(m, m.children, std::forward<As>(as)...);
  return finish(std::move(m), std::forward<As>(as)...);
}

// handleAt peer: records the laid-out box dispatch hands it (test
// capture only — production elements never mutate in handlers).
struct AtBox {
  const char* kind() const { return "AtBox"; }
  mutable Box seen{0, 0, 0, 0};
  Box place(Constraints c, const Engine&) const {
    return glim::toolkit::detail::resolveSize(50.0f, 20.0f, c);
  }
  bool handleAt(const Box& b, const Event& ev) const {
    if (ev.is<PointerDown>()) {
      seen = b;
      return true;
    }
    return false;
  }
};

}  // namespace

int main() {
  Store store;

  Block b = tap(lines("Play"), [] {}, primary(), pad(4));
  const Tap* bb = b.get<Tap>();
  assert(bb && bb->isPrimary && bb->padH == 4 && bb->padV == 4);
  assert(bb->children.size() == 1);
  const Lines* bl = bb->children[0].get<Lines>();
  assert(bl && bl->text == "Play");
  assert(!handle(lines("x"), Event::borrow(Activate{})));
  assert(!handle(lines("x"), Event::borrow(PointerMove{1, 1})));

  Block s = stack(lines("under"), column(lines("over")));
  const Stack* ss = s.get<Stack>();
  assert(ss && ss->children.size() == 2);
  Box sb = place(s, {0, 720, 0, 480});
  assert(sb.w > 0 && sb.h > 0);

  State<float> off = store.make<float>(0.0f);
  Block r = roll(horizontal(), scrollOffset(off), lines("a"), lines("b"));
  const Roll* rr = r.get<Roll>();
  assert(rr && rr->axis == kAxisHorizontal && rr->scrollOff);

  State<int> n = store.make<int>(41);
  Counter counter{n};
  Block tree = counter.view();
  const Column* col = tree.get<Column>();
  assert(col && col->children.size() == 2);
  assert(handle(col->children[1], Event::borrow(Activate{})));
  assert(*n == 42);

  State<int> sel = store.make<int>(1);
  Block m = myRadial(sel, gap(8), radius(6), tap(lines("x"), [] {}));
  const Radial* mm = m.get<Custom>()->as<Radial>();
  assert(mm && mm->gap == 8 && mm->radius == 6 && mm->children.size() == 2);
  Box mb = place(m, {0, 720, 0, 480});
  assert(mb.w > 0 && mb.h > 0);
  // Fallback metrics: headless (no Measurer in env) measures the
  // fixed advance — deterministic, and the golden binary proves the
  // real path.
  {
    Box fb3 = place(lines("MMM"), {0, 720, 0, 480});
    assert(fb3.w == 24 && fb3.h == 14);
  }
  // Custom children are transparent to focus: the tap inside Radial is found.
  assert(focusables(m, mb).size() == 1);

  // Seat: fixed reserve, kind-described (no record), focusable anchor
  // that claims tap/activate for the native field.
  Block f = seat(7, 688, 40, semantic("field"));
  const Seat* fs = f.get<Seat>();
  assert(fs && fs->id == 7 && fs->semantic == "field");
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

  // handleAt receives the node's laid-out box: tapping inside the
  // 50x20 peer reports the placed box back.
  {
    Block ab = toBlock(AtBox{});
    Box abox = place(ab, {0, 720, 0, 480});
    assert(dispatchPointer(ab, abox, 11, 6, Event::borrow(PointerDown{11, 6})));
    const AtBox* ap = ab.get<Custom>()->as<AtBox>();
    assert(ap && ap->seen.x == abox.x && ap->seen.y == abox.y);
    assert(ap->seen.w == 50 && ap->seen.h == 20);
  }

  // expand splits the remainder equally: 4 expanded in 720 with gap 8
  // share = (720 - 3*8) / 4 = 174.
  Block fr = row(
      gap(8), lines("a", expand()), lines("b", expand()), lines("c", expand()), lines("d", expand())
  );
  Box fb = place(fr, {0, 720, 0, 480});
  assert(fb.w == 720);
  {
    const Row* rp = fr.get<Row>();
    assert(rp && rp->children.size() == 4);
    Engine e;
    for (std::size_t i = 0; i < 4; ++i) {
      assert(rp->children[i].has<ExpandTag>());
      assert(rp->childBox(i, fb, e).w == 174);
    }
    Rec recr;
    record(fr, fb, recr);
    for (const auto& en : recr.entries) assert(en.kind != "Fill");
  }

  // Overflow spills: rigid children wider than budget keep natural size.
  Block ov = row(gap(0), lines("12345678901234567890"), lines("12345678901234567890"));
  Box ovb = place(ov, {0, 100, 0, 480});
  assert(ovb.w == 100);
  {
    Rec recr;
    record(ov, ovb, recr);
    assert(recr.entries[1].box.w == 160 && recr.entries[2].box.w == 160);
  }

  // Stack offers the same box to every child; paint walks forward,
  // hit walks reverse so children[0] is under and last is on top.
  Block modal = stack(lines("under"), lines("over"));
  Box modalBox = place(modal, {0, 720, 0, 480});
  {
    Rec recr;
    record(modal, modalBox, recr);
    assert(recr.entries.size() == 3);
    assert(recr.entries[1].text == "under" && recr.entries[2].text == "over");
    assert(recr.entries[1].box.x == recr.entries[2].box.x);
    assert(recr.entries[1].box.y == recr.entries[2].box.y);
    const Block* top = hit(modal, modalBox, 2, 2);
    assert(top && top->get<Lines>() && top->get<Lines>()->text == "over");
    const Block* miss = hit(modal, modalBox, 500, 400);
    assert(!miss);
  }

  // Roll measures content unbounded on its scroll axis, viewport clamps.
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
    const Block* top = hit(scrolled, scrolledBox, 2, 2);
    assert(top && top->get<Lines>() && top->get<Lines>()->text == "12345678901234567890");
  }

  // Clip-aware hit: points fully outside a Roll's viewport miss even
  // though the content spills there; partially visible controls still
  // hit. Paint pushes the same clip group, so dispatch agrees with
  // what the user sees.
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
    const Block* vis = hit(tall, tallBox, 4, 4);
    assert(vis && vis->get<Tap>());
    // y=120 sits inside a laid-out filler line but outside the
    // viewport: paint clips it, so hit must miss instead of
    // dispatching to an invisible line. Same for the buried tap.
    assert(!hit(tall, tallBox, 4, 120));
    assert(!hit(tall, tallBox, 4, 160));
    // Scrolled down by 60: filler 0 sits fully above the viewport
    // ([-46,-32)) and must miss, while filler 4 ([10,24) shifted)
    // is visible and still hits at y=20.
    State<float> down = store.make<float>(60.0f);
    Block moved = roll(vertical(), scrollOffset(down), tap(lines("top"), [] {}), list(10, filler));
    Box movedBox = place(moved, {0, 720, 0, 100});
    assert(!hit(moved, movedBox, 4, -40));
    const Block* part = hit(moved, movedBox, 4, 20);
    assert(part && part->get<Lines>());
  }

  // Wheel scroll: offset advances by the delta, clamps to
  // content-viewport, chains from an edge-clamped inner roll to the
  // outer one, and ignores points outside the viewport.
  {
    auto filler = [](int i) { return lines("filler" + std::to_string(i) + "-filler-filler"); };
    Screen scr;
    Engine e;
    State<float> outer = store.make<float>(0.0f);
    Block tree = roll(vertical(), scrollOffset(outer), list(12, filler));
    // 12 fillers x 14px = 168 content; viewport 100 → max 68.
    Box tb = place(tree, {0, 720, 0, 100});
    assert(scr.wheel(tree, tb, 4, 50, 0.0f, 30.0f, e));
    assert(*outer == 30.0f);
    // Overshoot clamps instead of spilling.
    assert(scr.wheel(tree, tb, 4, 50, 0.0f, 100.0f, e));
    assert(*outer == 68.0f);
    // At the edge there is nowhere to go: no paint requested.
    assert(!scr.wheel(tree, tb, 4, 50, 0.0f, 10.0f, e));
    // Scroll back up; zero delta is a no-op.
    assert(scr.wheel(tree, tb, 4, 50, 0.0f, -20.0f, e));
    assert(*outer == 48.0f);
    assert(!scr.wheel(tree, tb, 4, 50, 0.0f, 0.0f, e));
    // Outside the viewport: missed, offset untouched.
    assert(!scr.wheel(tree, tb, 4, 150, 0.0f, 10.0f, e));
    assert(*outer == 48.0f);
    // No offset state wired: unscrollable, never consumes.
    Block bare = roll(vertical(), list(12, filler));
    Box bb = place(bare, {0, 720, 0, 100});
    assert(!scr.wheel(bare, bb, 4, 50, 0.0f, 10.0f, e));
    // Nested: inner horizontal roll takes dx, the outer vertical
    // takes dy at the same point.
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

  // Column expand splits vertical remainder equally.
  Block fc = column(gap(0), lines("a", expand()), lines("b", expand()));
  Box fcb = place(fc, {0, 720, 0, 100});
  assert(fcb.h == 100);
  {
    const Column* cp = fc.get<Column>();
    assert(cp && cp->children.size() == 2);
    Engine e;
    for (std::size_t i = 0; i < 2; ++i) {
      assert(cp->children[i].has<ExpandTag>());
      assert(cp->childBox(i, fcb, e).h == 50);
    }
  }

  // Semantic marks ride the record entries hooks for the a11y node map.
  Block st = tap(lines("x"), [] {}, semantic("ok"));
  {
    Rec recr;
    record(st, place(st, {0, 720, 0, 480}), recr);
    assert(recr.entries[0].kind == Tap::kKind);
    assert(recr.entries[0].mark == "ok");
  }

  // Pad is uniform or two-axis; Tap grows its touch target by it.
  Block p = tap(lines("ab"), [] {}, pad(6, 2));
  const Tap* pp = p.get<Tap>();
  assert(pp && pp->padH == 6 && pp->padV == 2);
  Box pb = place(p, {0, 720, 0, 480});
  assert(pb.w == 16 + 12 && pb.h == 14 + 4);

  // Row/Column pad insets content: layout grows by the inset and
  // children shift by it (outer screen margin without a painting
  // Card; zero pad keeps every existing box).
  {
    Block pc = column(gap(0), pad(16), lines("ab"));
    Box pcb = place(pc, {0, 720, 0, 480});
    assert(pcb.w == 16 + 32 && pcb.h == 14 + 32);
    const Column* pcp = pc.get<Column>();
    assert(pcp);
    Engine e;
    Box cb0 = pcp->childBox(0, pcb, e);
    assert(cb0.x == 16 && cb0.y == 16);
    Block pr = row(gap(0), pad(10, 4), lines("ab"));
    Box prb = place(pr, {0, 720, 0, 480});
    assert(prb.w == 16 + 20 && prb.h == 14 + 8);
    const Row* prp = pr.get<Row>();
    assert(prp);
    Box rb0 = prp->childBox(0, prb, e);
    assert(rb0.x == 10 && rb0.y == 4);
  }

  // Store revision: idle-skip signal. make() does not bump; every set
  // does (all four spellings); copies share the cell so either alias
  // bumps; independent stores track independently.
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

  // DirtyGate: toolkit-owned idle-skip policy. First check paints;
  // acknowledge silences until the revision moves; invalidate forces
  // (non-State pixel changes, future T7 tweens).
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

  // Screen: toolkit-owned interaction state. Pointer sets focus
  // silently and captures the target; a second finger never hijacks;
  // keyboard shows the ring; revision alone is the Frame signal.
  {
    Store hs;
    State<int> picked = hs.make<int>(-1);
    Block st = row(gap(0), tap(lines("a"), [picked] { picked = 0; }), tap(lines("b"), [picked] {
                     picked = 1;
                   }));
    Box sbox = place(st, {0, 720, 0, 480});
    Screen screen;
    assert(screen.focus == -1 && !screen.focusVisible);
    // First paint: gate fires with no state change.
    assert(screen.needsPaint(hs));
    screen.painted(hs);
    assert(!screen.needsPaint(hs));
    // PointerDown on the first tap: consumed, focus + capture land,
    // ring stays silent.
    assert(screen.pointerDown(st, sbox, 1, 1, 7));
    assert(screen.focus == 0 && screen.drag == 0 && screen.dragId == 7);
    assert(!screen.focusVisible);
    assert(*picked == 0);
    // Captured drag far outside routes (no re-hit-test) and defers:
    // Tap ignores moves, capture retained, no immediate paint.
    assert(!screen.pointerMove(st, sbox, 500, 400, true, 7));
    assert(screen.drag == 0 && *picked == 0);
    // Second finger while dragging is ignored, never hijacked.
    assert(!screen.pointerMove(st, sbox, 500, 400, true, 8));
    assert(screen.drag == 0);
    // Hover never dispatches.
    assert(!screen.pointerMove(st, sbox, 1, 1, false, 7));
    // Release clears the capture.
    screen.pointerUp(st, sbox, 500, 400);
    assert(screen.drag == -1);
    // The tap's write dirtied the gate (correct); re-baseline.
    assert(screen.needsPaint(hs));
    screen.painted(hs);
    // Missed down captures nothing and keeps previous focus.
    assert(!screen.pointerDown(st, sbox, 500, 400, 9));
    assert(screen.focus == 0 && screen.drag == -1);
    // Keyboard: nav moves focus, ring shows.
    assert(screen.key(st, sbox, Key::Next));
    assert(screen.focus == 1 && screen.focusVisible);
    // State write is the only automatic Frame signal.
    assert(!screen.needsPaint(hs));
    picked = 1;
    assert(screen.needsPaint(hs));
    screen.painted(hs);
    assert(!screen.needsPaint(hs));
  }

  // Wrap is opt-in: plain Lines spills single-line, wrapped Lines reflows.
  {
    // No wrap tag → single 160px line spills past a 100px budget (T1 lock).
    Block spill = lines("12345678901234567890");
    Box spb = place(spill, {0, 100, 0, 480});
    assert(spb.w == 160 && spb.h == 14);
    // wrap() → two visual lines at 100px (12 chars + 8 chars).
    Block wrapped = lines("12345678901234567890", wrap());
    Box wb = place(wrapped, {0, 100, 0, 480});
    assert(wb.w == 96 && wb.h == 28);
    Rec recr;
    record(wrapped, wb, recr);
    assert(recr.entries.size() == 2);
    assert(recr.entries[0].text == "123456789012" && recr.entries[0].box.x == 0);
    assert(recr.entries[1].text == "34567890");
    assert(recr.entries[1].box.y == 14);
    // Word wrap keeps words whole; long words split at char boundaries.
    Block words = lines("aa bb cc dd", wrap());
    Rec wr;
    record(words, place(words, {0, 40, 0, 480}), wr);
    assert(wr.entries.size() == 2);
    assert(wr.entries[0].text == "aa bb" && wr.entries[1].text == "cc dd");
    // Align offsets wrapped lines inside a wider box.
    Block centered = lines("hi", wrap(), alignCenter());
    Box cb = place(centered, {0, 720, 0, 480});
    Rec cr;
    record(centered, {0, 0, 100, 14}, cr);
    assert(cr.entries.size() == 1);
    assert(cr.entries[0].box.x == 42);  // (100 - 16) / 2
    Block right = lines("hi", wrap(), alignRight());
    Rec rr;
    record(right, {0, 0, 100, 14}, rr);
    assert(rr.entries[0].box.x == 84);
    // Ellipsis: bounded height truncates with "..." on the last line.
    Block trunc = lines("aa bb cc dd", wrap());
    Rec tr;
    record(trunc, {0, 0, 40, 14}, tr);
    assert(tr.entries.size() == 1);
    assert(tr.entries[0].text == "aa...");
    // Unbounded height never truncates (Roll spill witness).
    Block tall = lines("aa bb cc dd", wrap());
    Rec talr;
    record(tall, {0, 0, 40, kUnbounded}, talr);
    assert(talr.entries.size() == 2);
    assert(talr.entries[1].text == "cc dd");
    (void)cb;
    Rec zr;
    record(tall, {0, 0, 40, 0}, zr);
    assert(zr.entries.size() == 1);
    assert(zr.entries[0].text == "aa...");
  }

  // List: eager homogeneous Column; composes with Roll for scroll.
  {
    Block li = list(3, [](int i) { return lines("row" + std::to_string(i)); });
    const List* lp = li.get<List>();
    assert(lp && lp->children.size() == 3);
    Box lb = place(li, {0, 720, 0, 480});
    assert(lb.h == 42 && lb.w == 32);
    Rec lr;
    record(li, lb, lr);
    assert(lr.entries.size() == 4);  // List + 3 rows
    assert(lr.entries[0].kind == List::kKind);
    // List of taps stays focusable in order.
    State<int> picked = store.make<int>(-1);
    Block lt = list(2, [picked](int i) {
      return tap(lines("t" + std::to_string(i)), [picked, i] { picked = i; });
    });
    Box ltb = place(lt, {0, 720, 0, 480});
    auto lf = focusables(lt, ltb);
    assert(lf.size() == 2);
    assert(deliverKey(lt, ltb, 0, Key::Next) == 1);
    assert(deliverKey(lt, ltb, 1, Key::Activate) == 1 && *picked == 1);
    // Negative count → empty list, no crash.
    Block le = list(-1, [](int) { return lines("x"); });
    assert(le.get<List>() && le.get<List>()->children.empty());
  }

  {
    struct Ping {};
    struct Bell {
      State<int> rings;
      const char* kind() const { return "Bell"; }
      Box place(Constraints c, const Engine&) const { return detail::resolveSize(10, 10, c); }
      bool handle(const Event& ev) const {
        if (ev.is<Ping>()) {
          rings = *rings + 1;
          return true;
        }
        return false;
      }
    };
    State<int> n = store.make<int>(0);
    Block tree = column(gap(0), tap(lines("x"), [] {}), Block{Custom{Bell{n}}});
    Box tbox = place(tree, {0, 720, 0, 480});
    assert(broadcastEvent(tree, tbox, Event::borrow(Ping{})));
    assert(*n == 1);
    Block bare = tap(lines("x"), [] {});
    Box bbox = place(bare, {0, 720, 0, 480});
    assert(!broadcastEvent(bare, bbox, Event::borrow(Ping{})));
    assert(!handle(bare, Event::borrow(Ping{})));
    struct MissProbe {
      int* n;
      const char* kind() const { return "MissProbe"; }
      Box place(Constraints c, const Engine&) const { return detail::resolveSize(10, 10, c); }
      bool handle(const Event&) const {
        ++(*n);
        return false;
      }
    };
    int missCount = 0;
    Block filled = Block{Custom{Tagged<ExpandTag, MissProbe>{MissProbe{&missCount}, ExpandTag{}}}};
    Box fbox = place(filled, {0, 720, 0, 480});
    assert(!broadcastEvent(filled, fbox, Event::borrow(Ping{})));
    assert(missCount == 1);
  }

  // Custom slot tags ride any element with no opt-in: the parent reads Pin,
  // the element never mentions it.
  {
    State<int> pc = store.make<int>(0);
    Block pinned = row(gap(0), tap(lines("a"), [pc] { pc = 1; }, Pin{}), lines("b"));
    const Row* rp = pinned.get<Row>();
    assert(rp && rp->children.size() == 2);
    assert(rp->children[0].has<Pin>());
    assert(rp->children[0].tag<Pin>() != nullptr);
    assert(!rp->children[1].has<Pin>());
    assert(handle(rp->children[0], Event::borrow(Activate{})));
    assert(*pc == 1);
  }

  // Inherited tags flow down through oblivious middles: TintProbe reads Tint
  // set two levels up; nearest scope wins.
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
  assert(fi == 0);  // clamped at start
  fi = moveFocus(grid, gridBox, 8, FocusDir::Next);
  assert(fi == 8);  // clamped at end
  Box fbox = focusBox(grid, gridBox, 4);
  assert(fbox.x == 8 && fbox.y == 14);

  // Key delivery: arrows move like moveFocus, Activate handles in place.
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
    // Empty tree: stays put, no crash.
    Block empty;
    Box ebox = place(empty, {0, 720, 0, 480});
    assert(deliverKey(empty, ebox, 0, Key::Right) == 0);
    assert(deliverKey(empty, ebox, 0, Key::Activate) == 0);
  }

  // Pointer capture: focusAt maps a point to its focusable; the
  // captured index routes events anywhere (no re-hit-test), and a
  // stale index reads false so the caller falls back to hit-testing.
  {
    State<int> tapped = store.make<int>(0);
    Block cap = row(gap(0), tap(lines("a"), [tapped] { tapped = 1; }), tap(lines("b"), [tapped] {
                      tapped = 2;
                    }));
    Box cbox = place(cap, {0, 720, 0, 480});
    int ai = focusAt(cap, cbox, 1, 1);
    assert(ai == 0);
    assert(focusAt(cap, cbox, 500, 400) == -1);
    // Captured move far outside still routes (true) but a Tap
    // ignores moves — count unchanged, capture retained.
    assert(dispatchCaptured(cap, cbox, ai, Event::borrow(PointerMove{500, 400, true})));
    assert(*tapped == 0);
    // Captured down activates in place, wherever the pointer now is.
    assert(dispatchCaptured(cap, cbox, ai, Event::borrow(PointerDown{500, 400})));
    assert(*tapped == 1);
    assert(!dispatchCaptured(cap, cbox, 7, Event::borrow(PointerDown{1, 1})));
    assert(!dispatchCaptured(cap, cbox, -1, Event::borrow(PointerDown{1, 1})));
  }

  // Focus ring: expanded overlay box, painted last by paintFocusRing.
  {
    Box ring = focusRingBox(grid, gridBox, 4);
    assert(ring.x == fbox.x - 2 && ring.y == fbox.y - 2);
    assert(ring.w == fbox.w + 4 && ring.h == fbox.h + 4);
    // Out-of-range index: zero box.
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

  // Scoped state: get-or-make by key, stable across builds, set bumps
  // the revision for the host tree cache. String, type-tag, and
  // positional spellings share one map; child scopes namespace.
  {
    Store scoped;
    Engine se;
    se.store = &scoped;
    Scope root = rootScope(se);
    auto a = root.state("level", 0.4f);
    auto b = root.state("level", 0.9f);
    assert(*a == 0.4f && *b == 0.4f);  // second init ignored
    a = 0.7f;
    assert(*b == 0.7f && scoped.revision() == 1);
    struct LevelTag {};
    auto t1 = root.state<LevelTag>(1);
    auto t2 = root.state<LevelTag>(2);
    assert(*t1 == 1 && *t2 == 1);
    auto p1 = root.next<int>(10);
    auto p2 = root.next<int>(20);
    assert(*p1 == 10 && *p2 == 20);
    // Fresh scope, same order: positional reopens the same cells.
    Scope root2 = rootScope(se);
    assert(*root2.next<int>(99) == 10 && *root2.next<int>(99) == 20);
    // Child scope namespaces: same key, distinct cells.
    auto c1 = root.child("toolbar").state("notice", std::string("a"));
    auto c2 = root.state("notice", std::string("b"));
    assert(*c1 == "a" && *c2 == "b");
    c1 = std::string("c");
    assert(*c1 == "c" && *c2 == "b" && scoped.revision() == 2);
  }

  // Struct state with explicit immutable update: one copy, one bump,
  // no copy-out/mutate/assign dance at the call site.
  {
    struct Filters {
      int span = 0;
      int mode = 0;
    };
    Store states;
    State<Filters> filters = states.make<Filters>();
    const std::uint64_t base = states.revision();
    filters.update([](Filters& f) { f.span = 2; });
    assert(filters->span == 2 && filters->mode == 0);
    assert(states.revision() == base + 1);
    filters.update([](Filters& f) {
      f.span = -1;
      f.mode = 1;
    });
    assert(filters->span == -1 && filters->mode == 1);
    assert(states.revision() == base + 2);
  }

  // Theme: dynamic keys — override, nested inner-wins, sibling isolation.
  // Probes read via themeColor in place (Themed::place/childBox push
  // the scope) and in paint (the walk threads the scoped engine).
  // User space defines its own keys (themeKey); the toolkit owns
  // only window/ink/type/focus builtins.
  {
    constexpr static ThemeKey kTestAccent = themeKey("test.accent");
    constexpr std::uint32_t kTestAccentDefault = 0xff5a1fff;
    struct AccentProbe {
      const char* kind() const { return "AccentProbe"; }
      std::uint32_t* out = nullptr;
      Box place(Constraints c, const Engine& e) const {
        *out = themeColor(e, kTestAccent, kTestAccentDefault);
        return detail::resolveSize(10, 10, c);
      }
    };
    // Override wins over ambient.
    {
      std::uint32_t seen = 0;
      Block t = themed(themeColor(kTestAccent, 0x11223344), toBlock(AccentProbe{&seen}));
      place(t, {0, 720, 0, 480});
      assert(seen == 0x11223344);
    }
    // Ambient fallback when unscoped.
    {
      std::uint32_t seen = 0;
      place(toBlock(AccentProbe{&seen}), {0, 720, 0, 480});
      assert(seen == kTestAccentDefault);
    }
    // Nested overrides compose inner-wins.
    {
      std::uint32_t seen = 0;
      Block t = themed(
          themeColor(kTestAccent, 0x11111111),
          themed(themeColor(kTestAccent, 0x22222222), toBlock(AccentProbe{&seen}))
      );
      place(t, {0, 720, 0, 480});
      assert(seen == 0x22222222);
    }
    // Siblings are isolated.
    {
      std::uint32_t sa = 0, sb = 0;
      Block t =
          row(gap(0),
              themed(themeColor(kTestAccent, 0xaaaaaaaa), toBlock(AccentProbe{&sa})),
              themed(themeColor(kTestAccent, 0xbbbbbbbb), toBlock(AccentProbe{&sb})));
      place(t, {0, 720, 0, 480});
      assert(sa == 0xaaaaaaaa && sb == 0xbbbbbbbb);
    }
    // Paint sees the scope too: ink override reaches the Text op.
    {
      OpsRecorder rec;
      struct Feed {
        std::uint32_t id = 0;
        bool foreign = false;
      } feed;
      Block t = themed(inkOnLight(0x123456ff), lines("hi"));
      Box tb = place(t, {0, 720, 0, 480});
      paintTree(t, tb, Engine{}, rec, feed);
      bool foundText = false;
      for (const auto& o : rec.ops) {
        if (o.kind == Op::Kind::Text) {
          foundText = true;
          assert(o.rgba == 0x123456ff);
        }
      }
      assert(foundText);
    }
  }

  std::cout << "toolkit tests ok\n";
  return 0;
}
