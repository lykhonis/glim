#pragma once

#include <vector>

#include "glim/toolkit/Event.h"
#include "glim/toolkit/Layout.h"
#include "glim/toolkit/Place.h"

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
int deliverKey(const Block& root, const Box& box, int current, Key key, const Engine& e = Engine{});

// Depth-first focusables: handling elements (Tap, handleful Customs).
// Customs exposing children (a `children` member or eachChild, plus
// childBox, e.g. `Card`) are transparent and recursed; a handling Custom WITH children contributes
// itself (pre-order) plus its children, so container actions stay reachable.
// Lifetime: returned pointers borrow from `root` — same rule as `hit` in
// Place.h. The tree must outlive the vector; never call on a temporary.
std::vector<const Block*> focusables(const Block& root, const Box& box, const Engine& e = Engine{});

// Spatial/linear focus move over focusables laid out in box.
// Returns the new index (clamped, stays put when empty).
int moveFocus(
    const Block& root, const Box& box, int current, FocusDir dir, const Engine& e = Engine{}
);

// Box of the i-th focusable in the same walk moveFocus uses.
Box focusBox(const Block& root, const Box& box, int index, const Engine& e = Engine{});

// Hit → focus index: index of the focusable containing (x, y), or -1.
// The pointer-capture entry point: PointerDown stores it, pressed moves
// route via dispatchCaptured instead of re-hit-testing.
int focusAt(const Block& root, const Box& box, float x, float y, const Engine& e = Engine{});

// Captured-pointer routing: delivers ev to the i-th focusable through
// handleAt with its current box, wherever the pointer now is. Unlike
// dispatchPointer the return reports routing, not consumption: a
// routed-but-ignored move (slider value unchanged) must NOT read as
// stale, or slow drags would drop the capture — callers paint captured
// moves on the Frame tick either way. Returns false only when i is out
// of range (stale capture after a layout change — the caller drops the
// capture and falls back to dispatchPointer). Pointer-id matching
// stays caller-side (shell concept; toolkit pointer events carry no id).
bool dispatchCaptured(
    const Block& root, const Box& box, int index, const Event& ev, const Engine& e = Engine{}
);

// Focus-ring overlay. The ring composes as an overlay: the caller paints
// the tree first, then the ring last (on top).
// focusRingBox expands the focused box by pad on all sides (default 2px).
// Out-of-range index (or empty focus set) yields a zero box / no entry.
inline constexpr const char* kFocusRingKind = "FocusRing";
Box focusRingBox(
    const Block& root, const Box& box, int index, float pad = 2.0f, const Engine& e = Engine{}
);

}  // namespace toolkit
}  // namespace glim
