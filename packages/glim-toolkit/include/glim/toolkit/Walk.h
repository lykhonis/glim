#pragma once

// One canonical traversal: pre-order, boxes threaded; exit pairs every entered node.
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

// Borrowed a11y id; tree must outlive pointer, entries copy it at emission.
inline std::optional<std::reference_wrapper<const std::string>> semanticOf(const Block& b) {
  if (auto c = b.get<Custom>()) {
    if (auto s = c->get().semanticPtr()) return s;
    return std::nullopt;
  }
  return std::visit([](const auto& x) { return elementSemanticPtr(x); }, b.v);
}

// Unmarked entries borrow the node's id; explicit marks are preserved.
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
  if (auto s = semanticOf(b)) {
    for (std::size_t i = base; i < r.entries.size(); ++i) {
      if (r.entries[i].mark.empty()) r.entries[i].mark = s->get();
    }
  }
}

// True when the node has traversable children in this walk.
inline bool walkHasChildren(const Block& b) {
  if (auto c = b.get<Custom>()) return c->get().childCount() > 0;
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

// Either spelling counts: handle or handleAt.
inline bool canHandle(const Block& b) {
  if (auto c = b.get<Custom>()) return c->get().handles();
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

// Wrapper-transparent: slot tags never break the scroll clip.
inline bool clipsChildren(const Block& b) {
  if (auto c = b.get<Custom>()) return c->get().clipsChildren();
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
// Unmarked ops borrow the node id; pixels ignore marks.
struct MarkFrame {
  AnySink* inner = nullptr;
  std::optional<std::reference_wrapper<const std::string>> mark;

  const std::string& markOf(const std::string& m) const {
    return (m.empty() && mark && !mark->get().empty()) ? mark->get() : m;
  }
  void fill(const Box& b, std::uint32_t rgba, const std::string& m) {
    inner->fill(b, rgba, markOf(m));
  }
  void fillRounded(const Box& b, int radius, std::uint32_t rgba, const std::string& m) {
    inner->fillRounded(b, radius, rgba, markOf(m));
  }
  void fillGradient(
      const Box& b, int radius, std::uint32_t top, std::uint32_t bottom, const std::string& m
  ) {
    inner->fillGradient(b, radius, top, bottom, markOf(m));
  }
  void fillRadial(
      const Box& b, int radius, std::uint32_t center, std::uint32_t edge, const std::string& m
  ) {
    inner->fillRadial(b, radius, center, edge, markOf(m));
  }
  void fillChamfer(const Box& b, float cut, std::uint32_t rgba, const std::string& m) {
    inner->fillChamfer(b, cut, rgba, markOf(m));
  }
  void strokeChamfer(
      const Box& b, float cut, float width, std::uint32_t rgba, const std::string& m
  ) {
    inner->strokeChamfer(b, cut, width, rgba, markOf(m));
  }
  void stroke(const Box& b, int radius, float width, std::uint32_t rgba, const std::string& m) {
    inner->stroke(b, radius, width, rgba, markOf(m));
  }
  void arc(
      float cx,
      float cy,
      float radius,
      float thickness,
      float startDeg,
      float sweepDeg,
      std::uint32_t rgba,
      const std::string& m
  ) {
    inner->arc(cx, cy, radius, thickness, startDeg, sweepDeg, rgba, markOf(m));
  }
  void polyline(
      const float* xy,
      std::size_t n,
      float width,
      float dotPitch,
      std::uint32_t rgba,
      const std::string& m
  ) {
    inner->polyline(xy, n, width, dotPitch, rgba, markOf(m));
  }
  void text(
      float x, float y, const std::string& str, std::uint32_t rgba, float size, const std::string& m
  ) {
    inner->text(x, y, str, rgba, size, markOf(m));
  }
  void textStyled(
      float x,
      float y,
      const std::string& str,
      std::uint32_t rgba,
      float size,
      float condense,
      float weight,
      const std::string& m
  ) {
    inner->textStyled(x, y, str, rgba, size, condense, weight, markOf(m));
  }
  void image(const Box& b, std::uint32_t id, bool foreign, const std::string& m) {
    inner->image(b, id, foreign, markOf(m));
  }
  void pushGlass(
      const Box& b,
      int radius,
      bool clear,
      std::uint8_t edge,
      std::uint32_t tint,
      std::uint8_t material
  ) {
    inner->pushGlass(b, radius, clear, edge, tint, material);
  }
  void pushClip(const Box& b) { inner->pushClip(b); }
  void pushBlend(BlendMode m) { inner->pushBlend(m); }
  void pushOpacity(float opacity) { inner->pushOpacity(opacity); }
  void pushTransform(float dx, float dy) { inner->pushTransform(dx, dy); }
  void pushRotate(float angleDeg, float cx, float cy) { inner->pushRotate(angleDeg, cx, cy); }
  void pushShadow(
      float sigma, float ox, float oy, std::uint32_t color, float expand, float opacity = 1.f
  ) {
    inner->pushShadow(sigma, ox, oy, color, expand, opacity);
  }
  void pushEffect(
      const Box& b, int radius, std::uint32_t kind, std::uint32_t color, float intensity
  ) {
    inner->pushEffect(b, radius, kind, color, intensity);
  }
  void pop() { inner->pop(); }
  void slot(const Box& b, std::uint32_t id) { inner->slot(b, id); }
};

inline AnySink markSink(
    AnySink& inner, std::optional<std::reference_wrapper<const std::string>> mark, MarkFrame& frame
) {
  frame.inner = &inner;
  frame.mark = mark;
  AnySink a;
  a.sink = &frame;
  a.feed = inner.feed;
  a.layer = inner.layer;
  // Entries go through the inner sink's own feed.
  a.entryFn = [](void* s, void*, const Rec::Entry& en) {
    const auto* fr = static_cast<const MarkFrame*>(s);
    if (en.mark.empty() && fr->mark && !fr->mark->get().empty()) {
      Rec::Entry patched = en;
      patched.mark = fr->mark->get();
      fr->inner->entry(patched);
    } else {
      fr->inner->entry(en);
    }
  };
  bindOps<MarkFrame>(a);
  return a;
}

template <class S, class F>
void paintSelf(const Block& b, const Box& box, S& sink, F& feed, const Engine& e) {
  AnySink any = wrapSink(sink, feed);
  auto sem = semanticOf(b);
  AnySink* use = &any;
  MarkFrame frame;
  AnySink marked;
  if (sem && !sem->get().empty()) {
    marked = markSink(any, sem, frame);
    use = &marked;
  }
  if (auto c = b.get<Custom>()) {
    if (c->get().paints()) {
      c->get().paint(box, *use, e);
      return;
    }
    Rec tmp;
    c->get().record(box, tmp);
    for (const auto& en : tmp.entries) use->entry(en);
    return;
  }
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_paint<T>::value) {
          el.paint(box, *use, e);
        } else if constexpr (has_record<T>::value) {
          Rec tmp;
          el.record(box, tmp);
          for (const auto& en : tmp.entries) use->entry(en);
        }
      },
      b.v
  );
}
// Tap-like builtins own their region, even spilled; Customs never claim.
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

// Layout-only wrappers never claim, so stacked invisibles can't steal taps.
inline bool hitParticipates(const Block& b) {
  if (auto c = b.get<Custom>()) {
    if (c->get().handles() || c->get().paints()) return true;
    return c->get().childCount() == 0;
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

// Borrowed view; tree must outlive the walk.
inline ThemeScope themeScopeOf(const Block& b) {
  if (auto c = b.get<Custom>()) return c->get().themeScope();
  return std::visit(
      [](const auto& x) -> ThemeScope {
        using T = std::decay_t<decltype(x)>;
        if constexpr (has_theme_entries<T>::value) return x.themeScope();
        return {};
      },
      b.v
  );
}

template <class Enter, class Exit>
void walk(const Block& b, const Box& box, const Engine& e, Enter&& enter, Exit&& exit) {
  detail::AutoGuard g(e.store ? &e.store->get() : nullptr);
  // Push scope, not replace: store/measurer carry through, only theme changes.
  ThemeScope tv = themeScopeOf(b);
  ThemeScope held = tv;
  Env node{&held, &typeid(ThemeScope), e.env};
  Engine scoped = e;
  scoped.env = node;
  const Engine& de = tv.count == 0 ? e : scoped;
  bool descend = enter(b, box, de);
  if (descend) {
    if (auto c = b.get<Custom>()) {
      std::size_t k = 0;
      c->get().visit(box, de, [&](const Block& child, Box cb) {
        detail::PathPush p(static_cast<int>(k++));
        walk(child, cb, de, enter, exit);
      });
    } else {
      std::visit(
          [&](const auto& el) {
            using T = std::decay_t<decltype(el)>;
            if constexpr (has_children<T>::value) {
              std::size_t i = 0;
              eachChildOf(el, [&](const Block& child) {
                detail::PathPush p(static_cast<int>(i));
                walk(child, el.childBox(i, box, de), de, enter, exit);
                ++i;
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
