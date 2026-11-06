#include "test_helpers.h"

using namespace glim::toolkit;

void test_scoped() {
  {
    Store scoped;
    Engine se;
    se.store = scoped;
    Scope root = rootScope(se);
    auto a = root.state("level", 0.4f);
    auto b = root.state("level", 0.9f);
    assert(*a == 0.4f && *b == 0.4f);
    a = 0.7f;
    assert(*b == 0.7f && scoped.revision() == 1);
    struct LevelTag {};
    auto t1 = root.state<LevelTag>(1);
    auto t2 = root.state<LevelTag>(2);
    assert(*t1 == 1 && *t2 == 1);
    auto p1 = root.next<int>(10);
    auto p2 = root.next<int>(20);
    assert(*p1 == 10 && *p2 == 20);
    Scope root2 = rootScope(se);
    assert(*root2.next<int>(99) == 10 && *root2.next<int>(99) == 20);
    auto c1 = root.child("toolbar").state("notice", std::string("a"));
    auto c2 = root.state("notice", std::string("b"));
    assert(*c1 == "a" && *c2 == "b");
    c1 = std::string("c");
    assert(*c1 == "c" && *c2 == "b" && scoped.revision() == 2);
  }

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
}
