#pragma once

#include <iosfwd>
#include <string>
#include <typeinfo>
#include <vector>

#include "glim/toolkit/Event.h"
#include "glim/toolkit/Layout.h"

namespace glim {
namespace toolkit {

struct Block;

struct Env {
  const void* ptr = nullptr;
  const std::type_info* type = nullptr;
  const Env* parent = nullptr;
};

Box place(const Block& b, Constraints c);

bool handle(const Block& b, const Event& ev);

bool handleAt(const Block& b, const Box& box, const Event& ev);

struct Engine {
  const Env* env = nullptr;
  Box place(const Block& b, Constraints c) const;
  void record(const Block& b, const Box& box, Rec& r) const;
  template <class T>
  const T* get() const {
    for (const Env* p = env; p; p = p->parent) {
      if (p->type && *p->type == typeid(T)) return static_cast<const T*>(p->ptr);
    }
    return nullptr;
  }
};

// Every traversal takes the caller's Engine — boxes re-derive through
// childBox on each walk, so the walk engine must match the placement
// engine. Bare Engine{} is the fixed-advance fallback (headless
// tests); the app passes its measuring engine everywhere.
void record(const Block& b, const Box& box, Rec& r, const Engine& e = Engine{});
// Lifetime: the returned pointer borrows from `b` — the tree must outlive
// the pointer. Call and use immediately; never store past the tree's
// lifetime, and never call on a temporary.
const Block* hit(const Block& b, const Box& box, float x, float y, const Engine& e = Engine{});

bool dispatchPointer(
    const Block& b, const Box& box, float x, float y, const Event& ev, const Engine& e = Engine{}
);

bool broadcastEvent(const Block& b, const Box& box, const Event& ev, const Engine& e = Engine{});

void dump(
    std::ostream& os, const Block& b, const Box& box, const Engine& e = Engine{}, int indent = 0
);

namespace detail {

float clampF(float v, float lo, float hi);
Box measureText(const std::string& text, Constraints c, const Engine& e);
// Greedy word wrap at maxW (unbounded → single line). Words longer than a
// line split at char boundaries. Spaces collapse to one separator.
std::vector<std::string> wrapText(const std::string& text, float maxW);
// Truncate wrapped lines to maxH with a "..." ellipsis on the last visible
// line, keeping at least one line.
std::vector<std::string> truncateLines(std::vector<std::string> lines, float maxW, float maxH);
// Full visual layout for an allotted box: wrap (iff wrap) then truncate.
std::vector<std::string> layoutLines(const std::string& text, float maxW, float maxH, bool wrap);
// Tight proposal forces exact size; loose max never truncates —
// rigid children spill past the budget, containers clamp their own box.
Box resolveSize(float naturalW, float naturalH, Constraints c);
// Shrink a proposal by pad chrome: max reduced by d (floored at 0,
// unbounded stays unbounded), min reduced by d (floored at 0). Tight
// proposals stay tight. Lets containers measure content in the inset box
// so `place` agrees with `record`/`childBox` (which lay out inset).
Constraints inset(Constraints c, float dx, float dy);

struct Flow {
  Box size;
  std::vector<Box> children;
};

// Opt-in cross-axis centering (see CenterTag): Row centers children
// vertically, Column horizontally. Applied by the element after flow
// so place and childBox agree from one call each.
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
