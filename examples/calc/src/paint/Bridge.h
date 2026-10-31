#pragma once

// Block tree → paint sink walk. Mirrors the dump/hit/focus traversal
// (eachChild + childBox, Custom via visit) so paint visits exactly the
// boxes layout produced. Per-node visuals come from the node's OWN Rec
// entry (first entry of a scoped record — the same code path as paint,
// zero per-element paint code, wrappers transparent). Structure (glass
// groups, clips) comes from tags + kinds.
//
// Slot wrappers (expand, glass, user tags) never block the walk:
// record/visit/childBox all delegate through Tagged, and paint reads
// data (kind/text/radius) rather than structs. The escape hatch is
// has_paint_with: an UNWRAPPED Custom element exposing
// paint(box, sink, engine) paints itself; wrapped customs fall back to
// Rec paint (correct for every builtin + Button/Card/Veil/Video).

#include <cstddef>
#include <type_traits>
#include <utility>

#include "elements/Button.h"
#include "elements/Card.h"
#include "elements/Veil.h"
#include "elements/Video.h"
#include "glim/toolkit/Toolkit.h"
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

inline bool isTransparentKind(const std::string& k) {
  return k == "Row" || k == "Column" || k == "Stack" || k == "List" ||
         k == "Empty" || k == "Tap" || k == "Tap.primary" ||
         isRollKind(k);
}

template <class S>
void paintChildren(
    const Block& b,
    const Box& box,
    const Engine& e,
    S& sink,
    VideoFeed& feed
);

template <class S>
void paintTree(
    const Block& b,
    const Box& box,
    const Engine& e,
    S& sink,
    VideoFeed& feed
) {
  if (tryCustomPaint(b, box, e, sink)) return;
  Rec tmp;
  e.record(b, box, tmp);
  if (tmp.entries.empty()) return;
  const Rec::Entry& self = tmp.entries[0];

  // Frosted-surface recipe (matches the glass example: chrome groups
  // are empty, content rides sibling-after). A glass-tagged node paints
  // its base fill first (frosted by the chrome sampling the backdrop),
  // then an empty chrome group, then text/children sharp on top.
  // Content must never sit inside the chrome isolate — the software
  // rasterizer drops fills there, and the reference pattern keeps
  // chrome groups empty.
  bool glassed = false;
  bool glassClear = false;
  if (const GlassTag* g = b.tag<GlassTag>()) {
    glassed = true;
    glassClear = g->clear;
  }
  bool clipped = false;
  if (isRollKind(self.kind)) {
    sink.pushClip(box);
    clipped = true;
  }

  const std::string& k = self.kind;
  auto paintFill = [&] {
    if (k == "Card") {
      sink.fillRounded(box, self.radius, Tokens::kCardFill);
    } else if (k == "Button" || k == "Button.primary") {
      sink.fillRounded(
          box,
          Tokens::kButtonRadius,
          k == "Button.primary" ? Tokens::kButtonPrimaryFill
                                : Tokens::kButtonFill
      );
    } else if (k == "Veil") {
      sink.fill(box, Tokens::kScrim);
    } else if (k == "Video") {
      sink.blitVideo(box, feed);
    }
  };
  auto paintText = [&] {
    if (k == "Button" || k == "Button.primary") {
      float tx = box.x + static_cast<float>(Tokens::kButtonPadH);
      float ty = box.y + (box.h - Tokens::kTypeLine) * 0.5f;
      sink.text(
          tx,
          ty,
          self.text,
          k == "Button.primary" ? Tokens::kInkOnDark
                                : Tokens::kInkOnLight
      );
    } else if (k == "Lines") {
      for (const auto& en : tmp.entries)
        sink.text(en.box.x, en.box.y, en.text, Tokens::kInkOnLight);
    }
  };
  // Transparent kinds (Row/Column/Stack/List/Empty/Tap/Roll): no self
  // paint. Unknown kinds: ignored (forward-compat for the archive
  // path).
  if (glassed) {
    paintFill();
    sink.pushGlass(box, self.radius, glassClear);
    sink.pop();
    paintText();
  } else {
    paintFill();
    paintText();
  }

  paintChildren(b, box, e, sink, feed);
  if (clipped) sink.pop();
}

template <class S>
void paintChildren(
    const Block& b,
    const Box& box,
    const Engine& e,
    S& sink,
    VideoFeed& feed
) {
  if (const Custom* c = b.get<Custom>()) {
    if (c->childCount() == 0) return;
    c->visit(box, e, [&](const Block& child, Box cb) {
      detail::paintTree(child, cb, e, sink, feed);
    });
    return;
  }
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_children<T>::value) {
          std::size_t i = 0;
          el.eachChild([&](const Block& child) {
            detail::paintTree(
                child, el.childBox(i++, box, e), e, sink, feed
            );
          });
        }
      },
      b.v
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

// Focus ring composes as an overlay: caller paints the tree first, then
// the ring last (on top) — same order as recordFocusRing appends last.
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
