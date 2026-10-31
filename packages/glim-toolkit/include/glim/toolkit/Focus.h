#pragma once

#include <vector>

#include "glim/toolkit/Event.h"
#include "glim/toolkit/Layout.h"

namespace glim {
namespace toolkit {

struct Block;
struct Engine;

// Host key input. DPAD/arrows + Tab order + activation; the host shell maps
// its key events onto these and calls deliverKey. No host key codes leak in.
enum class FocusDir { Next, Prev, Up, Down, Left, Right };

// Key delivery: focused block handles KeyPress first; ignored arrows move
// focus via moveFocus, Activate is handled in place. Returns the new focus
// index (clamped, stays put when empty, unchanged when consumed).
int deliverKey(const Block& root, const Box& box, int current, Key key);

// Depth-first focusables: handling elements (Tap, handleful Customs).
// Customs exposing children (eachChild + childBox, e.g. `Card`) are
// transparent and recursed; a handling Custom WITH children contributes
// itself (pre-order) plus its children, so container actions stay reachable.
// Lifetime: returned pointers borrow from `root` — same rule as `hit` in
// Place.h. The tree must outlive the vector; never call on a temporary.
std::vector<const Block*> focusables(const Block& root, const Box& box);

// Spatial/linear focus move over focusables laid out in box.
// Returns the new index (clamped, stays put when empty).
int moveFocus(const Block& root, const Box& box, int current, FocusDir dir);

// Box of the i-th focusable in the same walk moveFocus uses.
Box focusBox(const Block& root, const Box& box, int index);

// Focus-ring overlay. The ring composes as an overlay: the caller records the
// tree first, then appends the ring entry last (on top) via recordFocusRing.
// focusRingBox expands the focused box by pad on all sides (default 2px).
// Out-of-range index (or empty focus set) yields a zero box / no entry.
inline constexpr const char* kFocusRingKind = "FocusRing";
Box focusRingBox(const Block& root, const Box& box, int index, float pad = 2.0f);
void recordFocusRing(Rec& rec, const Block& root, const Box& box, int index, float pad = 2.0f);

}  // namespace toolkit
}  // namespace glim
