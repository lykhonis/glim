#include "test_helpers.h"

using namespace glim::toolkit;

void test_text() {
  Store store;
  {
    // Single 160px line spills past a 100px budget.
    Block spill = lines("12345678901234567890");
    Box spb = place(spill, {0, 100, 0, 480});
    assert(spb.w == 160 && spb.h == 14);
    // Two lines at 100px (12 chars + 8 chars).
    Block wrapped = lines("12345678901234567890", wrap());
    Box wb = place(wrapped, {0, 100, 0, 480});
    assert(wb.w == 96 && wb.h == 28);
    Rec recr;
    record(wrapped, wb, recr);
    assert(recr.entries.size() == 2);
    assert(recr.entries[0].text == "123456789012" && recr.entries[0].box.x == 0);
    assert(recr.entries[1].text == "34567890");
    assert(recr.entries[1].box.y == 14);
    Block words = lines("aa bb cc dd", wrap());
    Rec wr;
    record(words, place(words, {0, 40, 0, 480}), wr);
    assert(wr.entries.size() == 2);
    assert(wr.entries[0].text == "aa bb" && wr.entries[1].text == "cc dd");
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
    Block trunc = lines("aa bb cc dd", wrap());
    Rec tr;
    record(trunc, {0, 0, 40, 14}, tr);
    assert(tr.entries.size() == 1);
    assert(tr.entries[0].text == "aa...");
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

  {
    Block li = list(3, [](int i) { return lines("row" + std::to_string(i)); });
    auto lp = li.get<List>();
    assert(lp && lp->get().children.size() == 3);
    Box lb = place(li, {0, 720, 0, 480});
    assert(lb.h == 42 && lb.w == 32);
    Rec lr;
    record(li, lb, lr);
    assert(lr.entries.size() == 4);  // List + 3 rows
    assert(lr.entries[0].kind == List::kKind);
    State<int> picked = store.make<int>(-1);
    Block lt = list(2, [picked](int i) {
      return tap(lines("t" + std::to_string(i)), [picked, i] { picked = i; });
    });
    Box ltb = place(lt, {0, 720, 0, 480});
    auto lf = focusables(lt, ltb);
    assert(lf.size() == 2);
    assert(deliverKey(lt, ltb, 0, Key::Next) == 1);
    assert(deliverKey(lt, ltb, 1, Key::Activate) == 1 && *picked == 1);
    Block le = list(-1, [](int) { return lines("x"); });
    assert(le.get<List>() && le.get<List>()->get().children.empty());
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
      int& n;
      const char* kind() const { return "MissProbe"; }
      Box place(Constraints c, const Engine&) const { return detail::resolveSize(10, 10, c); }
      bool handle(const Event&) const {
        ++n;
        return false;
      }
    };
    int missCount = 0;
    Block filled = Block{Custom{Tagged<ExpandTag, MissProbe>{MissProbe{missCount}, ExpandTag{}}}};
    Box fbox = place(filled, {0, 720, 0, 480});
    assert(!broadcastEvent(filled, fbox, Event::borrow(Ping{})));
    assert(missCount == 1);
  }
}
