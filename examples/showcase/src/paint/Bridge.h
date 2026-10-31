#pragma once

// Block tree → paint sink walk, now on the core traversal
// (toolkit::walk): pre-order, boxes threaded, Custom-transparent.
// Per-node visuals come from the node's OWN Rec entries (first entry
// for containers, all entries for leaves — the same rule as dump),
// emitted through sink.entry; grouping (glass chrome, roll clip) rides
// enter/exit pairing. Slot wrappers (expand, glass, user tags) never
// block the walk: record/visit/childBox all delegate through Tagged.
//
// Frosted-surface recipe (see Demo.h): entries first (fill-only by
// convention), then an EMPTY chrome group, then children sharp on top.
// Text-bearing glass nodes would frost their text — glass tags go on
// fill-only containers (Card), a documented constraint, not a check.

#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

#include "elements/Button.h"
#include "elements/Card.h"
#include "elements/Veil.h"
#include "elements/Video.h"
#include "glim/toolkit/Toolkit.h"
#include "glim/toolkit/Walk.h"
#include "paint/Ops.h"
#include "paint/VideoFeed.h"

namespace glim {
namespace toolkit {
namespace paint {

template <class T, class S>
std::enable_if_t<has_paint_with<T, S>::value, bool> maybePaint(
    const T* t, const Box& box, S& sink, const Engine& e
) {
  t->paint(box, sink, e);
  return true;
}

template <class T, class S>
std::enable_if_t<!has_paint_with<T, S>::value, bool>
maybePaint(const T*, const Box&, S&, const Engine&) {
  return false;
}

template <class S>
bool tryCustomPaint(
    const Block& b, const Box& box, const Engine& e, S& sink
) {
  const Custom* c = b.get<Custom>();
  if (!c) return false;
  if (const auto* t = c->as<Button>())
    if (maybePaint(t, box, sink, e)) return true;
  if (const auto* t = c->as<Card>())
    if (maybePaint(t, box, sink, e)) return true;
  if (const auto* t = c->as<Veil>())
    if (maybePaint(t, box, sink, e)) return true;
  if (const auto* t = c->as<Video>())
    if (maybePaint(t, box, sink, e)) return true;
  return false;
}

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
  std::vector<const Block*> clips;  // nodes holding an open clip group
  walk(
      b,
      box,
      e,
      [&](const Block& n, const Box& bx) -> bool {
        if (tryCustomPaint(n, bx, e, sink)) return false;
        Rec tmp;
        e.record(n, bx, tmp);
        if (tmp.entries.empty()) return false;
        bool leaf = !walkHasChildren(n);
        auto emitOwn = [&] {
          if (leaf) {
            for (const auto& en : tmp.entries) sink.entry(en, feed);
          } else {
            sink.entry(tmp.entries[0], feed);
          }
        };
        if (const GlassTag* g = n.tag<GlassTag>()) {
          emitOwn();
          sink.pushGlass(bx, tmp.entries[0].radius, g->clear);
          sink.pop();
        } else {
          emitOwn();
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
template <class S>
void paintFocusRing(
    const Block& root, const Box& box, int index, S& sink
) {
  Box rb = focusRingBox(root, box, index);
  if (rb.w <= 0 || rb.h <= 0) return;
  sink.stroke(rb, 4, 2.0f, Tokens::kFocusRing);
}

}  // namespace paint
}  // namespace toolkit
}  // namespace glim
