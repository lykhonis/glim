#pragma once

#include <cmath>
#include <iosfwd>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include "glim/toolkit/Event.h"
#include "glim/toolkit/Layout.h"
#include "glim/toolkit/State.h"

namespace glim {
namespace toolkit {

struct Block;

using BlockRef = std::optional<std::reference_wrapper<const Block>>;

struct Env {
  const void* ptr = nullptr;
  const std::type_info* type = nullptr;
  std::optional<std::reference_wrapper<const Env>> parent;
};

Box place(const Block& b, Constraints c);

bool handle(const Block& b, const Event& ev);

bool handleAt(const Block& b, const Box& box, const Event& ev);

struct Engine {
  std::optional<std::reference_wrapper<const Env>> env;
  StoreRef store;
  Box place(const Block& b, Constraints c) const;
  void record(const Block& b, const Box& box, Rec& r) const;
  template <class T>
  std::optional<std::reference_wrapper<const T>> get() const {
    for (auto p = env; p; p = p->get().parent) {
      const Env& n = p->get();
      if (n.type && *n.type == typeid(T)) return std::cref(*static_cast<const T*>(n.ptr));
    }
    return std::nullopt;
  }
};

// Walk engine must match placement engine; bare Engine{} is the fixed-advance fallback.
void record(const Block& b, const Box& box, Rec& r, const Engine& e = Engine{});
// Lifetime: pointer borrows from `b`; never store past the tree or call on a temporary.
BlockRef hit(const Block& b, const Box& box, float x, float y, const Engine& e = Engine{});
// Hit plus laid-out box in one walk. Null block + zero box on miss.
std::pair<BlockRef, Box> hitWithBox(
  const Block& b, const Box& box, float x, float y, const Engine& e = Engine{}
);

bool dispatchPointer(
  const Block& b, const Box& box, float x, float y, const Event& ev, const Engine& e = Engine{}
);

bool broadcastEvent(const Block& b, const Box& box, const Event& ev, const Engine& e = Engine{});

void dump(
  std::ostream& os, const Block& b, const Box& box, const Engine& e = Engine{}, int indent = 0
);

namespace detail {

float clampF(float v, float lo, float hi);
// One home for hit/scroll geometry (Hit.cpp, Screen::wheel share it).
bool boxContains(const Box& b, float x, float y);
Box intersectBox(const Box& a, const Box& b);
// Clip stack parallels the walk: Roll children hit only inside the clip.
struct ClipStack {
  std::vector<Box> clips;
  std::vector<bool> pushed;
  void enter(const Block& n, const Box& bx);
  void exit();
  bool visible(float x, float y) const;
};
// 0.5 logical snaps 1px chrome at 2x; layout boxes stay untouched.
inline float snapHalf(float v) { return std::floor(v * 2.0f + 0.5f) / 2.0f; }
// Tight proposal forces exact size; loose max never truncates.
Box resolveSize(float naturalW, float naturalH, Constraints c);
// Shrink a proposal by pad chrome: max/min reduced by d (floored at 0, unbounded stays unbounded).
Constraints inset(Constraints c, float dx, float dy);

struct Flow {
  Box size;
  std::vector<Box> children;
};

// Opt-in cross-axis centering; place and childBox agree from one call each.
inline void centerRow(Flow& f) {
  for (auto& b : f.children) b.y += (f.size.h - b.h) * 0.5f;
}
inline void centerColumn(Flow& f) {
  for (auto& b : f.children) b.x += (f.size.w - b.w) * 0.5f;
}

Flow flowRow(const std::vector<Block>& children, int gap, Constraints c, const Engine& e);
Flow flowColumn(const std::vector<Block>& children, int gap, Constraints c, const Engine& e);
Flow flowStack(const std::vector<Block>& children, Constraints c, const Engine& e);

}  // namespace detail

}  // namespace toolkit
}  // namespace glim
