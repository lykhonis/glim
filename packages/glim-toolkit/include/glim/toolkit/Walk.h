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
inline bool canHandle(const Block& b) {
  if (const Custom* c = b.get<Custom>()) return c->handles();
  bool found = false;
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_handle<T>::value) found = true;
      },
      b.v
  );
  return found;
}

// True when the Custom arm paints itself (opt-in escape hatch for
// visuals entries can't carry). Paint is written once against AnySink
// and runs on every sink — pixels on interpreters, ops on recorders —
// so custom-painted elements archive without a fallback path.
inline bool paintsCustom(const Block& b) {
  if (const Custom* c = b.get<Custom>()) return c->paints();
  return false;
}
inline void paintCustom(const Block& b, const Box& box, AnySink& sink, const Engine& e) {
  if (const Custom* c = b.get<Custom>()) c->paint(box, sink, e);
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
