#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "glim/toolkit/Layout.h"

namespace glim {
namespace toolkit {

struct Block;

Box place(const Block& b, Constraints c);
void record(const Block& b, const Box& box, Rec& r);
void dump(std::ostream& os, const Block& b, int indent = 0);
// Lifetime: the returned pointer borrows from `b` — the tree must outlive
// the pointer. Call and use immediately (e.g. `fire(*hit(...))`); never
// store past the tree's lifetime, and never call on a temporary.
const Block* hit(const Block& b, const Box& box, float x, float y);

// Fires the action on a Tap arm, a transparent Fill, or a fireable Custom.
// Returns false for everything else (containers included: hit first).
bool fire(const Block& b);

struct Engine {
  Box place(const Block& b, Constraints c) const { return glim::toolkit::place(b, c); }
  void record(const Block& b, const Box& box, Rec& r) const {
    glim::toolkit::record(b, box, r);
  }
};

namespace detail {

float clampF(float v, float lo, float hi);
Box measureText(const std::string& text, Constraints c);
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
