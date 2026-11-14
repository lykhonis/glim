#pragma once

#include <vector>

#include "glim/toolkit/Event.h"
#include "glim/toolkit/Layout.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

struct Block;
struct Engine;

// Host key input: DPAD/arrows + Tab order + activation.
enum class FocusDir { Next, Prev, Up, Down, Left, Right };

// Key delivery returns the new focus index.
int deliverKey(const Block& root, const Box& box, int current, Key key, const Engine& e = Engine{});

// Depth-first focusables; handling Customs with children contribute themselves plus children.
// Lifetime: pointers borrow from `root`; tree must outlive the vector.
std::vector<const Block*> focusables(const Block& root, const Box& box, const Engine& e = Engine{});

// Spatial/linear focus move; returns the new index.
int moveFocus(
  const Block& root, const Box& box, int current, FocusDir dir, const Engine& e = Engine{}
);

// Box of the i-th focusable in the same walk moveFocus uses.
Box focusBox(const Block& root, const Box& box, int index, const Engine& e = Engine{});

// Hit → focus index; PointerDown stores it, pressed moves route via dispatchCaptured.
int focusAt(const Block& root, const Box& box, float x, float y, const Engine& e = Engine{});

// Captured-pointer routing: return is routing, not consumption, so ignored moves keep capture.
// Returns false only when i is out of range (caller falls back to dispatchPointer).
bool dispatchCaptured(
  const Block& root, const Box& box, int index, const Event& ev, const Engine& e = Engine{}
);

// Focus-ring overlay, painted last (on top).
// focusRingBox expands the focused box by pad (default 2px).
inline constexpr const char* kFocusRingKind = "FocusRing";
Box focusRingBox(
  const Block& root, const Box& box, int index, float pad = 2.0f, const Engine& e = Engine{}
);

}  // namespace toolkit
}  // namespace glim
