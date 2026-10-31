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
static_assert(!has_children<Empty>::value, "");
static_assert(!has_children<Lines>::value, "");
static_assert(has_inner_box<Tap>::value, "");
static_assert(!has_inner_box<Row>::value, "");

namespace glim {
namespace toolkit {

struct Radial : WithGap, WithRadius {
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
  template <class F>
  void eachChild(F&& f) const {
    for (const auto& child : children) f(child);
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
  void record(const Box& b, Rec& r, const Engine& e) const {
    r.push("Radial", b);
    for (std::size_t i = 0; i < children.size(); ++i) e.record(children[i], childBox(i, b, e), r);
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
  void record(const Box& box, Rec& rec, const Engine&) const { rec.push(kKind, box); }
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
        tap(lines("Inc"), [c] { c = *c + 1; }, primary())
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
  // Custom children are transparent to focus: the tap inside Radial is found.
  assert(focusables(m, mb).size() == 1);
  {
    Box tb = focusBox(m, mb, 0);
    assert(
        dispatchPointer(m, mb, tb.x + 1, tb.y + 1, Event::borrow(PointerDown{tb.x + 1, tb.y + 1}))
    );
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
    assert(lp && lp->items.size() == 3);
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
    assert(le.get<List>() && le.get<List>()->items.empty());
  }

  {
    struct Ping {};
    struct Bell {
      State<int> rings;
      Box place(Constraints c, const Engine&) const { return detail::resolveSize(10, 10, c); }
      void record(const Box& box, Rec& rec, const Engine&) const { rec.push("Bell", box); }
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
      Box place(Constraints c, const Engine&) const { return detail::resolveSize(10, 10, c); }
      void record(const Box& box, Rec& rec, const Engine&) const { rec.push("Miss", box); }
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

  // Focus ring: expanded overlay box appended last by recordFocusRing.
  {
    Box ring = focusRingBox(grid, gridBox, 4);
    assert(ring.x == fbox.x - 2 && ring.y == fbox.y - 2);
    assert(ring.w == fbox.w + 4 && ring.h == fbox.h + 4);
    Rec recr;
    record(grid, gridBox, recr);
    std::size_t base = recr.entries.size();
    recordFocusRing(recr, grid, gridBox, 4);
    assert(recr.entries.size() == base + 1);
    assert(recr.entries.back().kind == kFocusRingKind);
    assert(recr.entries.back().box.x == ring.x && recr.entries.back().box.y == ring.y);
    // Out-of-range index: zero box, no entry.
    assert(focusRingBox(grid, gridBox, 99).w == 0);
    Rec empty;
    recordFocusRing(empty, grid, gridBox, 99);
    assert(empty.entries.empty());
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

  std::cout << "toolkit tests ok\n";
  return 0;
}
