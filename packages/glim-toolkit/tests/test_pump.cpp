#include "test_helpers.h"

using namespace glim::toolkit;

void test_pump() {
  {
    Store st;
    State<float> v = st.make<float>(0.0f);
    int calls = 0;
    Store::Watch vt = st.watchCell(v, [&](float now) {
      ++calls;
      v = now;
      return now < 2.0f;
    });
    assert(vt.live() && st.valuesLive());
    st.stepValues(1.0f);
    assert(*v == 1.0f && st.valuesLive() && calls == 1);
    st.stepValues(2.0f);
    assert(*v == 2.0f && !st.valuesLive() && calls == 2);
    assert(!vt.live());
    st.stepValues(3.0f);
    assert(calls == 2);
    State<float> w = st.make<float>(5.0f);
    {
      Store::Watch wt = st.watchCell(w, [&](float) {
        ++calls;
        return true;
      });
      assert(wt.live() && st.valuesLive());
    }
    assert(!st.valuesLive());
    st.stepValues(4.0f);
    assert(calls == 2 && *w == 5.0f);
    int frames = 0;
    Store::Watch ft = st.watchFrame([&](float) {
      ++frames;
      return frames < 3;
    });
    assert(ft.live() && st.valuesLive());
    st.stepValues(0.0f);
    st.stepValues(1.0f);
    assert(frames == 2 && st.valuesLive());
    st.stepValues(2.0f);
    assert(frames == 3 && !st.valuesLive());
    assert(!ft.live());
    st.stepValues(3.0f);
    assert(frames == 3);
    int manual = 0;
    Store::Watch mt = st.watchFrame([&](float) {
      ++manual;
      return true;
    });
    st.stepValues(4.0f);
    assert(manual == 1);
    mt.reset();
    assert(!mt.live());
    st.stepValues(5.0f);
    assert(manual == 1 && !st.valuesLive());
    {
      Store::Watch orphan;
      {
        Store inner;
        orphan = inner.watchFrame([](float) { return true; });
        assert(orphan.live() && inner.valuesLive());
      }
      assert(!orphan.live());
    }
    {
      Store cycled;
      struct Holder {
        Store::Watch w;
      };
      auto h = std::make_shared<Holder>();
      h->w = cycled.watchFrame([h](float) { return true; });
      assert(cycled.valuesLive());
    }
    {
      Store ms;
      int ticks = 0;
      Store::Watch a = ms.watchFrame([&](float) {
        ++ticks;
        return true;
      });
      Store::Watch b = std::move(a);
      assert(!a.live());
      assert(b.live() && ms.valuesLive());
      ms.stepValues(0.0f);
      assert(ticks == 1);
      Store::Watch c;
      c = std::move(b);
      assert(!b.live() && c.live() && ms.valuesLive());
      ms.stepValues(1.0f);
      assert(ticks == 2);
    }
  }
}
