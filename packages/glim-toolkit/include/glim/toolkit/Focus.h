#pragma once

#include <vector>

#include "glim/toolkit/Layout.h"

namespace glim {
namespace toolkit {

struct Block;
struct Engine;

enum class FocusDir { Next, Prev, Up, Down, Left, Right };

// Depth-first focusables: Tap arms plus fireable Customs, laid out in `box`.
// Customs exposing children (eachChild + childBox, e.g. `Card`) are
// transparent and recursed; a fireable Custom WITH children contributes
// itself (pre-order) plus its children, so container actions stay reachable.
// Lifetime: returned pointers borrow from `root` — same rule as `hit` in
// Place.h. The tree must outlive the vector; never call on a temporary.
std::vector<const Block*> focusables(const Block& root, const Box& box);

// Spatial/linear focus move over focusables laid out in box.
// Returns the new index (clamped, stays put when empty).
int moveFocus(const Block& root, const Box& box, int current, FocusDir dir);

// Box of the i-th focusable in the same walk moveFocus uses.
Box focusBox(const Block& root, const Box& box, int index);

}  // namespace toolkit
}  // namespace glim
