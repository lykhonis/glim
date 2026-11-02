#pragma once

// Block tree → paint sink walk on the core traversal
// (toolkit::walk): pre-order, boxes threaded, Custom-transparent.
// Per-node visuals come from the node's OWN Rec entries emitted
// through sink.entry; grouping (glass chrome, roll clip) rides
// enter/exit pairing. Slot wrappers (expand, glass, user tags) never
// block the walk: record/visit/childBox all delegate through Tagged.
// Custom-painted elements (Custom::paints) render through an AnySink
// and prune their subtree; everything else flows as entries.
//
// Frosted-surface recipe: entries first (fill-only by convention),
// then an EMPTY chrome group, then children sharp on top.
// Text-bearing glass nodes would frost their text — glass tags go on
// fill-only containers (Card), a documented constraint, not a check.

#include <cstddef>
#include <vector>

#include "glim/toolkit/Toolkit.h"
#include "glim/toolkit/Walk.h"
#include "paint/Ops.h"
#include "paint/VideoFeed.h"

namespace glim {
namespace toolkit {
namespace paint {

namespace detail {

inline bool isRollKind(const std::string& k) {
  return k == "Roll.h" || k == "Roll.v";
}

template <class S>
void paintTree(
    const Block& b,
    const Box& box,
    const Engine& e,
    S& sink,
    VideoFeed& feed
) {
  AnySink any = wrapSink(sink, feed);
  std::vector<const Block*> clips;  // nodes holding an open clip group
  walk(
      b,
      box,
      e,
      [&](const Block& n, const Box& bx) -> bool {
        if (paintsCustom(n)) {
          paintCustom(n, bx, any, e);
          return false;
        }
        // Seat reconcile: the arm reserves a host native — the bridge
        // turns its box into a structural slot (never an entry). The
        // subtree prunes here; Seats are leaves.
        if (const Seat* seat = n.get<Seat>()) {
          sink.slot(bx, seat->id);
          return false;
        }
        Rec tmp;
        describeSelf(n, bx, tmp);
        if (tmp.entries.empty()) return false;
        // Own entries first (fill-only by convention), then an empty
        // chrome group, then children sharp on top. Text-bearing glass
        // nodes would frost their text — glass tags go on fill-only
        // containers (Card), a documented constraint, not a check.
        if (const GlassTag* g = n.tag<GlassTag>()) {
          for (const auto& en : tmp.entries) sink.entry(en, feed);
          sink.pushGlass(bx, tmp.entries[0].radius, g->clear);
          sink.pop();
        } else {
          for (const auto& en : tmp.entries) sink.entry(en, feed);
        }
        if (isRollKind(tmp.entries[0].kind)) {
          sink.pushClip(bx);
          clips.push_back(&n);
        }
        return true;
      },
      [&](const Block& n, const Box&) {
        if (!clips.empty() && clips.back() == &n) {
          clips.pop_back();
          sink.pop();
        }
      }
  );
}

}  // namespace detail

template <class S>
void paintTree(
    const Block& b,
    const Box& box,
    const Engine& e,
    S& sink,
    VideoFeed& feed
) {
  detail::paintTree(b, box, e, sink, feed);
}

// Focus ring composes as an overlay: the caller paints the tree first,
// then the ring last (on top) — same order as recordFocusRing appends.
//
// Modern policy is focus-visible: pointer sets focus silently (no
// ring), keyboard shows it. Desktop users never see a stuck teal box
// on click; keyboard/a11y users keep a high-contrast ring. The test
// path passes visible=true explicitly; the runtime threads its
// modality flag.
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
  sink.stroke(rb, 4, 2.0f, Tokens::kFocusRing);
}

}  // namespace paint
}  // namespace toolkit
}  // namespace glim
