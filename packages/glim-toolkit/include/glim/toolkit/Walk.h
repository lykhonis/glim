#pragma once

// One canonical traversal. Every tree consumer (dump, hit, focus,
// broadcast, paint) walks through here: pre-order, boxes threaded via
// a `children` member (or eachChild) + childBox, Custom transparent
// via visit. Element authors never write recursion; they implement
// record()/handle() and call walk.
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
#include "glim/toolkit/theme/Keys.h"

namespace glim {
namespace toolkit {

// Single-source a11y id for any node: variant arms via
// elementSemanticPtr (unwraps Tagged), Custom peers via the vtable
// (same unwrapping, so slot-wrapped customs keep their id).
// Borrows from the tree — the tree must outlive the pointer, and
// Rec/Op entries copy it into owned strings at emission.
inline const std::string* semanticOf(const Block& b) {
  if (const Custom* c = b.get<Custom>()) {
    if (const std::string* s = c->semanticPtr()) return s;
    return nullptr;
  }
  return std::visit([](const auto& x) -> const std::string* { return elementSemanticPtr(x); }, b.v);
}

// Per-node self description (no recursion). Elements push only their
// own entries; elements without record() are described by their kind.
// Marks are attached here, not in elements: entries that arrive
// without a mark borrow the node's semanticOf id (copied into the
// owned Entry). Explicit marks are preserved.
// The walk accumulates subtrees; Engine::record (full subtree) is the
// walk over it. Single home for the "what is this node" query.
inline void describeSelf(const Block& b, const Box& box, Rec& r) {
  std::size_t base = r.entries.size();
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
  if (const std::string* s = semanticOf(b)) {
    for (std::size_t i = base; i < r.entries.size(); ++i) {
      if (r.entries[i].mark.empty()) r.entries[i].mark = *s;
    }
  }
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
// Mark-injecting sink wrapper: ops that arrive without a mark borrow
// the node's semanticOf id (copied into the owned Op at the final
// sink). Explicit marks pass through untouched. Pixel sinks ignore
// marks, so this costs one branch per op on recorders and nothing
// visible on pixels.
struct MarkFrame {
  const AnySink* inner = nullptr;
  const std::string* mark = nullptr;
};

inline AnySink markSink(const AnySink& inner, const std::string* mark, MarkFrame& frame) {
  frame.inner = &inner;
  frame.mark = mark;
  AnySink a;
  a.sink = &frame;
  a.feed = inner.feed;
  a.layer = inner.layer;
  a.entryFn = [](void* s, const void* f, const Rec::Entry& en) {
    const auto* fr = static_cast<const MarkFrame*>(s);
    if (en.mark.empty() && fr->mark && !fr->mark->empty()) {
      Rec::Entry patched = en;
      patched.mark = *fr->mark;
      fr->inner->entryFn(const_cast<void*>(fr->inner->sink), f, patched);
    } else {
      fr->inner->entryFn(const_cast<void*>(fr->inner->sink), f, en);
    }
  };
  a.fillFn = [](void* s, const Box& b, std::uint32_t rgba, const std::string& m) {
    const auto* fr = static_cast<const MarkFrame*>(s);
    const std::string& use = (m.empty() && fr->mark && !fr->mark->empty()) ? *fr->mark : m;
    fr->inner->fillFn(const_cast<void*>(fr->inner->sink), b, rgba, use);
  };
  a.fillRoundedFn = [](void* s, const Box& b, int radius, std::uint32_t rgba, const std::string& m
                    ) {
    const auto* fr = static_cast<const MarkFrame*>(s);
    const std::string& use = (m.empty() && fr->mark && !fr->mark->empty()) ? *fr->mark : m;
    fr->inner->fillRoundedFn(const_cast<void*>(fr->inner->sink), b, radius, rgba, use);
  };
  a.fillGradientFn = [](void* s,
                        const Box& b,
                        int radius,
                        std::uint32_t top,
                        std::uint32_t bottom,
                        const std::string& m) {
    const auto* fr = static_cast<const MarkFrame*>(s);
    const std::string& use = (m.empty() && fr->mark && !fr->mark->empty()) ? *fr->mark : m;
    fr->inner->fillGradientFn(const_cast<void*>(fr->inner->sink), b, radius, top, bottom, use);
  };
  a.textFn = [](void* s,
                float x,
                float y,
                const std::string& str,
                std::uint32_t rgba,
                float size,
                const std::string& m) {
    const auto* fr = static_cast<const MarkFrame*>(s);
    const std::string& use = (m.empty() && fr->mark && !fr->mark->empty()) ? *fr->mark : m;
    fr->inner->textFn(const_cast<void*>(fr->inner->sink), x, y, str, rgba, size, use);
  };
  a.imageFn = [](void* s, const Box& b, std::uint32_t id, bool foreign, const std::string& m) {
    const auto* fr = static_cast<const MarkFrame*>(s);
    const std::string& use = (m.empty() && fr->mark && !fr->mark->empty()) ? *fr->mark : m;
    fr->inner->imageFn(const_cast<void*>(fr->inner->sink), b, id, foreign, use);
  };
  a.pushGlassFn = [](void* s, const Box& b, int radius, bool clear) {
    const auto* fr = static_cast<const MarkFrame*>(s);
    fr->inner->pushGlassFn(const_cast<void*>(fr->inner->sink), b, radius, clear);
  };
  a.pushClipFn = [](void* s, const Box& b) {
    const auto* fr = static_cast<const MarkFrame*>(s);
    fr->inner->pushClipFn(const_cast<void*>(fr->inner->sink), b);
  };
  a.pushBlendFn = [](void* s, BlendMode m) {
    const auto* fr = static_cast<const MarkFrame*>(s);
    fr->inner->pushBlendFn(const_cast<void*>(fr->inner->sink), m);
  };
  a.popFn = [](void* s) {
    const auto* fr = static_cast<const MarkFrame*>(s);
    fr->inner->popFn(const_cast<void*>(fr->inner->sink));
  };
  a.slotFn = [](void* s, const Box& b, std::uint32_t id) {
    const auto* fr = static_cast<const MarkFrame*>(s);
    fr->inner->slotFn(const_cast<void*>(fr->inner->sink), b, id);
  };
  a.strokeFn =
      [](void* s, const Box& b, int radius, float width, std::uint32_t rgba, const std::string& m) {
        const auto* fr = static_cast<const MarkFrame*>(s);
        const std::string& use = (m.empty() && fr->mark && !fr->mark->empty()) ? *fr->mark : m;
        fr->inner->strokeFn(const_cast<void*>(fr->inner->sink), b, radius, width, rgba, use);
      };
  return a;
}

// Per-node self paint (no recursion). Prefers the element's own paint
// over its record entries; kind-only containers paint nothing;
// record-only peers keep the legacy entry path. Marks ride via the
// markSink wrapper (see above), so elements paint mark-free and the
// walk attaches the node's id once.
template <class S, class F>
void paintSelf(const Block& b, const Box& box, S& sink, F& feed, const Engine& e) {
  AnySink any = wrapSink(sink, feed);
  const std::string* sem = semanticOf(b);
  AnySink* use = &any;
  MarkFrame frame;
  AnySink marked;
  if (sem && !sem->empty()) {
    marked = markSink(any, sem, frame);
    use = &marked;
  }
  if (const Custom* c = b.get<Custom>()) {
    if (c->paints()) {
      c->paint(box, const_cast<AnySink&>(*use), e);
      return;
    }
    Rec tmp;
    c->record(box, tmp);
    for (const auto& en : tmp.entries) use->entry(en);
    return;
  }
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_paint<T>::value) {
          el.paint(box, const_cast<AnySink&>(*use), e);
        } else if constexpr (has_record<T>::value) {
          Rec tmp;
          el.record(box, tmp);
          for (const auto& en : tmp.entries) use->entry(en);
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

// Hit participation: visuals and handlers resolve; layout-only
// wrappers (no paint, no record, no handler — bare Columns, Themed
// scopes, stack positioning layers) never claim the point, so an
// invisible box stacked on top can't steal a tap from the control
// beneath it. Leaves always participate (Empty, Seat).
inline bool hitParticipates(const Block& b) {
  if (const Custom* c = b.get<Custom>()) {
    if (c->handles() || c->paints()) return true;
    return c->childCount() == 0;
  }
  bool visual = false, handle = false, kids = false;
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_paint<T>::value) visual = true;
        if constexpr (has_record<T>::value) visual = true;
        if constexpr (has_handle<T>::value || has_handle_at<T>::value) handle = true;
        if constexpr (has_children<T>::value) kids = true;
      },
      b.v
  );
  if (visual || handle) return true;
  return !kids;
}

// Borrowed theme entries for a subtree root: Custom peers via the
// vtable (Tagged-transparent, so slot-wrapped scopes keep working),
// variant arms via the same trait. Empty when the node scopes
// nothing. Entries borrow from the tree — the tree must outlive the
// walk.
inline ThemeScope themeScopeOf(const Block& b) {
  if (const Custom* c = b.get<Custom>()) return c->themeScope();
  return std::visit(
      [](const auto& x) -> ThemeScope {
        using T = std::decay_t<decltype(x)>;
        if constexpr (has_theme_entries<T>::value) return x.themeScope();
        return {nullptr, 0};
      },
      b.v
  );
}

template <class Enter, class Exit>
void walk(const Block& b, const Box& box, const Engine& e, Enter&& enter, Exit&& exit) {
  // Subtree theme scope: push the node's entries (no-op when empty)
  // and thread the scoped engine through enter, children, and exit —
  // place/record/paint/hit/focus all agree from this one hook. Push,
  // do not replace: the copy carries store/measurer through, so only
  // theme lookups change below.
  ThemeScope tv = themeScopeOf(b);
  ThemeScope held{tv.entries, tv.count};
  Env node{&held, &typeid(ThemeScope), e.env};
  Engine scoped = e;
  scoped.env = &node;
  const Engine& de = (tv.count == 0 || tv.entries == nullptr) ? e : scoped;
  bool descend = enter(b, box, de);
  if (descend) {
    if (const Custom* c = b.get<Custom>()) {
      c->visit(box, de, [&](const Block& child, Box cb) { walk(child, cb, de, enter, exit); });
    } else {
      std::visit(
          [&](const auto& el) {
            using T = std::decay_t<decltype(el)>;
            if constexpr (has_children<T>::value) {
              std::size_t i = 0;
              eachChildOf(el, [&](const Block& child) {
                walk(child, el.childBox(i++, box, de), de, enter, exit);
              });
            }
          },
          b.v
      );
    }
  }
  exit(b, box, de);
}

template <class F>
void walk(const Block& b, const Box& box, const Engine& e, F&& visit) {
  walk(
      b,
      box,
      e,
      [&](const Block& n, const Box& bx, const Engine&) -> bool {
        visit(n, bx);
        return true;
      },
      [](const Block&, const Box&, const Engine&) {}
  );
}

}  // namespace toolkit
}  // namespace glim
