#pragma once

// Block tree → sink emission on the canonical walk: pre-order, boxes
// threaded. Per-node visuals come from the node's own paint; grouping
// (glass chrome, roll clip) rides enter/exit pairing. Slot wrappers
// never block the walk. Peers without paint keep the legacy entry
// path; everything else flows as primitives. Sink-agnostic.
#include <cstddef>
#include <vector>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Focus.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Tokens.h"
#include "glim/toolkit/Walk.h"
#include "glim/toolkit/elements/Seat.h"
#include "glim/toolkit/theme/Keys.h"

namespace glim {
namespace toolkit {

namespace detail {

template <class S, class F>
void emitTree(const Block& b, const Box& box, const Engine& e, S& sink, F& feed) {
  std::vector<const Block*> clips;
  walk(
      b,
      box,
      e,
      [&](const Block& n, const Box& bx, const Engine& en) -> bool {
        // Seat reserves a host native: its box becomes a structural
        // slot, never an entry. Subtree prunes here; Seats are leaves.
        if (const Seat* seat = n.get<Seat>()) {
          sink.slot(bx, seat->id);
          return false;
        }
        paintSelf(n, bx, sink, feed, en);
        // Own visuals first (fill-only by convention), then an empty
        // chrome group, then children sharp on top. Glass tags go on
        // fill-only containers; text inside glass would frost.
        if (const GlassTag* g = n.tag<GlassTag>()) {
          sink.pushGlass(bx, g->radius, g->clear);
          sink.pop();
        }
        if (clipsChildren(n)) {
          sink.pushClip(bx);
          clips.push_back(&n);
        }
        return true;
      },
      [&](const Block& n, const Box&, const Engine&) {
        if (!clips.empty() && clips.back() == &n) {
          clips.pop_back();
          sink.pop();
        }
      }
  );
}

}  // namespace detail

template <class S, class F>
void paintTree(const Block& b, const Box& box, const Engine& e, S& sink, F& feed) {
  detail::emitTree(b, box, e, sink, feed);
}

// Focus ring is a paint-time overlay (never a record entry): paint
// the tree first, then the ring last (on top).
template <class S>
void paintFocusRing(
    const Block& root,
    const Box& box,
    int index,
    S& sink,
    const Engine& e = Engine{},
    bool visible = true
) {
  if (!visible) return;
  Box rb = focusRingBox(root, box, index, 2.0f, e);
  if (rb.w <= 0 || rb.h <= 0) return;
  sink.stroke(rb, 4, 2.0f, themeColor(e, kThemeFocusRing, Tokens::kFocusRing));
}

}  // namespace toolkit
}  // namespace glim
