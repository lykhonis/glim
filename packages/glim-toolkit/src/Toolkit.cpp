#include "glim/toolkit/Toolkit.h"

#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

// Engine entry points: visitation dispatches per node kind; every
// other concern lives in its own TU (TextLayout, Flow, Hit, Events,
// Focus, Screen).

Box Engine::place(const Block& b, Constraints c) const {
  return std::visit([&](const auto& x) -> Box { return x.place(c, *this); }, b.v);
}

void Engine::record(const Block& b, const Box& box, Rec& r) const {
  walk(
      b,
      box,
      *this,
      [&](const Block& n, const Box& bx, const Engine&) -> bool {
        describeSelf(n, bx, r);
        return true;
      },
      [](const Block&, const Box&, const Engine&) {}
  );
}

Box place(const Block& b, Constraints c) { return Engine{}.place(b, c); }

void record(const Block& b, const Box& box, Rec& r, const Engine& e) {
  Engine ee = e;
  ee.record(b, box, r);
}

}  // namespace toolkit
}  // namespace glim
