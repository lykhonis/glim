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
void record(const Block& b, const Box& box, Rec& r);
// Lifetime: the returned pointer borrows from `b` — the tree must outlive
// the pointer. Call and use immediately; never store past the tree's
// lifetime, and never call on a temporary.
const Block* hit(const Block& b, const Box& box, float x, float y);

bool handle(const Block& b, const Event& ev);

bool dispatchPointer(const Block& b, const Box& box, float x, float y, const Event& ev);

bool broadcastEvent(const Block& b, const Box& box, const Event& ev);

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

void dump(
    std::ostream& os, const Block& b, const Box& box, const Engine& e = Engine{}, int indent = 0
);

namespace detail {

float clampF(float v, float lo, float hi);
Box measureText(const std::string& text, Constraints c);
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

Flow flowRow(const std::vector<Block>& children, int gap, Constraints c, const Engine& e);
Flow flowColumn(const std::vector<Block>& children, int gap, Constraints c, const Engine& e);
Flow flowStack(const std::vector<Block>& children, Constraints c, const Engine& e);

}  // namespace detail

}  // namespace toolkit
}  // namespace glim
