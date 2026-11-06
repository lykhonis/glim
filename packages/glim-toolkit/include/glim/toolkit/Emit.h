#pragma once

// Block tree → sink emission on the canonical walk; sink-agnostic.
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
  std::vector<BlockRef> clips;
  walk(
      b,
      box,
      e,
      [&](const Block& n, const Box& bx, const Engine& en) -> bool {
        // Seat reserves a host native: structural slot, never an entry; Seats are leaves.
        if (auto seat = n.get<Seat>()) {
          sink.slot(bx, seat->get().id);
          return false;
        }
        paintSelf(n, bx, sink, feed, en);
        // Glass tags go on fill-only containers; text inside glass would frost.
        if (auto g = n.tag<GlassTag>()) {
          sink.pushGlass(bx, g->get().radius, g->get().clear);
          sink.pop();
        }
        if (clipsChildren(n)) {
          sink.pushClip(bx);
          clips.push_back(std::cref(n));
        }
        return true;
      },
      [&](const Block& n, const Box&, const Engine&) {
        if (!clips.empty() && clips.back() && &clips.back()->get() == &n) {
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

// Focus ring is a paint-time overlay: paint the tree first, then the ring last.
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
