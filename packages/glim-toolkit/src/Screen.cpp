#include <vector>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/Toolkit.h"
#include "glim/toolkit/elements/Roll.h"

namespace glim {
namespace toolkit {

bool Screen::pointerDown(
    const Block& tree, const Box& box, float x, float y, int pointerId, const Engine& engine
) {
  Event down = Event::borrow(PointerDown{x, y});
  if (dispatchPointer(tree, box, x, y, down, engine)) {
    // Capture only when the point truly lands in a focus box;
    // otherwise keep the previous focus and clear any stale capture
    // (a handled non-focusable must not keep routing moves to the
    // previous drag index).
    int hit = focusAt(tree, box, x, y, engine);
    if (hit >= 0) {
      focus = hit;
      drag = hit;
      dragId = pointerId;
    } else {
      drag = -1;
    }
    focusVisible = false;
    return true;
  }
  drag = -1;
  return false;
}

bool Screen::pointerUp(const Block& tree, const Box& box, float x, float y, const Engine& engine) {
  Event up = Event::borrow(PointerUp{x, y});
  drag = -1;
  return dispatchPointer(tree, box, x, y, up, engine);
}

bool Screen::pointerMove(
    const Block& tree,
    const Box& box,
    float x,
    float y,
    bool pressed,
    int pointerId,
    const Engine& engine
) {
  if (!pressed) return false;
  PointerMove move{x, y, pressed};
  Event moved = Event::borrow(move);
  if (drag >= 0) {
    // A second finger while dragging is ignored, never hijacked.
    if (pointerId != dragId) return false;
    // Routed (not consumed) keeps the drag; stale falls back to a
    // fresh hit-test.
    if (dispatchCaptured(tree, box, drag, moved, engine)) return false;
    drag = -1;
  }
  return dispatchPointer(tree, box, x, y, moved, engine);
}

bool Screen::wheel(
    const Block& tree, const Box& box, float x, float y, float dx, float dy, const Engine& engine
) {
  if (dx == 0 && dy == 0) return false;
  struct Candidate {
    const Roll* roll;
    Box box;
  };
  std::vector<Candidate> found;
  detail::ClipStack clip;
  walk(
      tree,
      box,
      engine,
      [&](const Block& n, const Box& bx, const Engine&) -> bool {
        // Ancestor clips gate candidacy; the node's own clip (pushed
        // below) trivially contains any point inside its box.
        bool parentVisible = clip.visible(x, y);
        clip.enter(n, bx);
        if (parentVisible) {
          if (const Roll* r = n.get<Roll>()) {
            if (r->scrollOff && detail::boxContains(bx, x, y)) found.push_back({r, bx});
          }
        }
        return true;
      },
      [&](const Block&, const Box&, const Engine&) { clip.exit(); }
  );
  // Innermost first; an edge-clamped inner roll chains outward.
  for (auto it = found.rbegin(); it != found.rend(); ++it) {
    const Roll* r = it->roll;
    const Box rb = it->box;
    // Strict axis match: a vertical wheel over a horizontal strip
    // bubbles to the outer vertical roll (browser behavior) instead
    // of trapping the gesture in the strip.
    const RollMetrics m = rollMetrics(*r, rb, engine);
    const float delta = m.horizontal ? dx : dy;
    if (delta == 0.0f || m.maxOff <= 0.0f) continue;
    const float next = detail::clampF(m.offset + delta, 0.0f, m.maxOff);
    if (next != m.offset) {
      r->scrollTo(next);
      return true;
    }
  }
  return false;
}

bool Screen::key(const Block& tree, const Box& box, Key key, const Engine& engine) {
  focus = deliverKey(tree, box, focus, key, engine);
  focusVisible = true;
  return true;
}

bool Screen::needsPaint(const Store& store) const { return dirty.check(store.revision()); }

void Screen::painted(const Store& store) { dirty.acknowledge(store.revision()); }

}  // namespace toolkit
}  // namespace glim
