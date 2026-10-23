#include <cassert>
#include <iostream>

#include "glim/toolkit/Toolkit.h"

using namespace glim::toolkit;

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

}  // namespace toolkit
}  // namespace glim

namespace {

struct Counter {
  glim::toolkit::State<int> count;
  glim::toolkit::Block view() const {
    auto c = count;
    using namespace glim::toolkit;
    return column(gap(8), lines("Count: " + std::to_string(*c)),
                  tap(lines("Inc"), [c] { c = *c + 1; }, primary()));
  }
};

template <class... As>
glim::toolkit::Block myRadial(glim::toolkit::State<int> sel, As&&... as) {
  using namespace glim::toolkit;
  Radial m;
  m.children.push_back(lines("sel=" + std::to_string(*sel)));
  configure(m, m.children, std::forward<As>(as)...);
  return Block{Custom{std::move(m)}};
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
  assert(!fire(lines("x")));

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
  assert(fire(col->children[1]));
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
    const Block* th = hit(m, mb, tb.x + 1, tb.y + 1);
    assert(th && fire(*th));
  }

  // Fill splits the remainder equally: 4 fills in 720 with gap 8
  // share = (720 - 3*8) / 4 = 174.
  Block fr = row(gap(8), fill(lines("a")), fill(lines("b")), fill(lines("c")), fill(lines("d")));
  Box fb = place(fr, {0, 720, 0, 480});
  assert(fb.w == 720);
  {
    Rec recr;
    record(fr, fb, recr);
    int fills = 0;
    for (const auto& e : recr.entries)
      if (e.kind == "Fill") {
        assert(e.box.w == 174);
        ++fills;
      }
    assert(fills == 4);
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
  Block scrolled = roll(horizontal(), scrollOffset(off2), lines("12345678901234567890"),
                        lines("12345678901234567890"));
  Box scrolledBox = place(scrolled, {0, 100, 0, 480});
  {
    Rec recr;
    record(scrolled, scrolledBox, recr);
    assert(recr.entries[1].box.x == -160);
    const Block* top = hit(scrolled, scrolledBox, 2, 2);
    assert(top && top->get<Lines>() && top->get<Lines>()->text == "12345678901234567890");
  }

  // Column fill splits vertical remainder equally.
  Block fc = column(gap(0), fill(lines("a")), fill(lines("b")));
  Box fcb = place(fc, {0, 720, 0, 100});
  assert(fcb.h == 100);
  {
    Rec recr;
    record(fc, fcb, recr);
    int fills = 0;
    for (const auto& e : recr.entries)
      if (e.kind == "Fill") {
        assert(e.box.h == 50);
        ++fills;
      }
    assert(fills == 2);
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

  // Focus: depth-first Tap order, spatial moves, ends clamp.
  Block grid = column(gap(0), row(gap(0), tap(lines("1"), [] {}), tap(lines("2"), [] {}),
                                        tap(lines("3"), [] {})),
                      row(gap(0), tap(lines("4"), [] {}), tap(lines("5"), [] {}),
                          tap(lines("6"), [] {})),
                      row(gap(0), tap(lines("7"), [] {}), tap(lines("8"), [] {}),
                          tap(lines("9"), [] {})));
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
  {
    State<int> tapped = store.make<int>(0);
    Block actionable = tap(lines("go"), [tapped] { tapped = 1; });
    const Block* hitTap = hit(actionable, place(actionable, {0, 720, 0, 480}), 1, 1);
    assert(hitTap && fire(*hitTap));
    assert(*tapped == 1);
  }

  std::cout << "toolkit tests ok\n";
  return 0;
}
