#pragma once

// One canonical traversal. Every tree consumer (dump, hit, focus,
// broadcast, paint) walks through here: pre-order, boxes threaded via
// eachChild + childBox, Custom transparent via visit. Element authors
// never write recursion; they implement record()/handle() and call walk.
//
// enter(b, box) returns whether to descend; exit(b, box) pairs every
// entered node (pop semantics: indent, clip/group pop). Claim rule:
// Tap-like builtins (innerBox) own their region — see claimsHit.

#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

// Per-node self description (no recursion). Elements push only their
// own entries; elements without record() are described by their kind.
// The walk accumulates subtrees; Engine::record (full subtree) is the
// walk over it. Single home for the "what is this node" query.
inline void describeSelf(const Block& b, const Box& box, Rec& r) {
  std::visit(
      [&](const auto& x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr (has_record<T>::value) {
          x.record(box, r);
        } else {
          r.push(x.kind(), box);
        }
      },
      b.v
  );
}

// True when the node has traversable children in this walk.
inline bool walkHasChildren(const Block& b) {
  if (const Custom* c = b.get<Custom>()) return c->childCount() > 0;
  bool found = false;
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_children<T>::value) found = true;
      },
      b.v
  );
  return found;
}

// True when the node handles events (duck-typed, no dispatch).
// Either spelling counts: plain handle or position-aware handleAt.
inline bool canHandle(const Block& b) {
  if (const Custom* c = b.get<Custom>()) return c->handles();
  bool found = false;
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_handle<T>::value || has_handle_at<T>::value) found = true;
      },
      b.v
  );
  return found;
}

// True when the node's children paint inside a clip group (Roll).
// Wrapper-transparent: Tagged forwards the flag, Custom carries it in
// the vtable, so slot tags never break the scroll clip.
inline bool clipsChildren(const Block& b) {
  if (const Custom* c = b.get<Custom>()) return c->clipsChildren();
  bool found = false;
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_clip_flag<T>::value) found = true;
      },
      b.v
  );
  return found;
}
// Per-node self paint (no recursion). Prefers the element's own paint
// over its record entries; kind-only containers paint nothing;
// record-only peers keep the legacy entry path.
template <class S, class F>
void paintSelf(const Block& b, const Box& box, S& sink, F& feed, const Engine& e) {
  AnySink any = wrapSink(sink, feed);
  if (const Custom* c = b.get<Custom>()) {
    if (c->paints()) {
      c->paint(box, any, e);
      return;
    }
    Rec tmp;
    c->record(box, tmp);
    for (const auto& en : tmp.entries) sink.entry(en, feed);
    return;
  }
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_paint<T>::value) {
          el.paint(box, any, e);
        } else if constexpr (has_record<T>::value) {
          Rec tmp;
          el.record(box, tmp);
          for (const auto& en : tmp.entries) sink.entry(en, feed);
        }
      },
      b.v
  );
}
// Tap-like builtins own their hit region (a hit on the child — even
// spilled past the chrome — resolves to the arm). Customs never claim.
inline bool claimsHit(const Block& b) {
  if (b.get<Custom>()) return false;
  bool found = false;
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_inner_box<T>::value) found = true;
      },
      b.v
  );
  return found;
}

template <class Enter, class Exit>
void walk(const Block& b, const Box& box, const Engine& e, Enter&& enter, Exit&& exit) {
  bool descend = enter(b, box);
  if (descend) {
    if (const Custom* c = b.get<Custom>()) {
      c->visit(box, e, [&](const Block& child, Box cb) { walk(child, cb, e, enter, exit); });
    } else {
      std::visit(
          [&](const auto& el) {
            using T = std::decay_t<decltype(el)>;
            if constexpr (has_children<T>::value) {
              std::size_t i = 0;
              el.eachChild([&](const Block& child) {
                walk(child, el.childBox(i++, box, e), e, enter, exit);
              });
            }
          },
          b.v
      );
    }
  }
  exit(b, box);
}

template <class F>
void walk(const Block& b, const Box& box, const Engine& e, F&& visit) {
  walk(
      b,
      box,
      e,
      [&](const Block& n, const Box& bx) -> bool {
        visit(n, bx);
        return true;
      },
      [](const Block&, const Box&) {}
  );
}

}  // namespace toolkit
}  // namespace glim
