#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "glim/toolkit/Event.h"
#include "glim/toolkit/Layout.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/theme/Keys.h"

namespace glim {
namespace toolkit {

template <class T, class = void>
struct has_handle : std::false_type {};
template <class T>
struct has_handle<
    T,
    std::void_t<decltype(std::declval<const T&>().handle(std::declval<const Event&>()))>>
    : std::is_same<decltype(std::declval<const T&>().handle(std::declval<const Event&>())), bool> {
};

// handleAt sees the laid-out box; dispatch prefers it, falls back to handle.
template <class T, class = void>
struct has_handle_at : std::false_type {};
template <class T>
struct has_handle_at<
    T,
    std::void_t<decltype(std::declval<const T&>()
                             .handleAt(std::declval<const Box&>(), std::declval<const Event&>()))>>
    : std::is_same<
          decltype(std::declval<const T&>()
                       .handleAt(std::declval<const Box&>(), std::declval<const Event&>())),
          bool> {};

template <class T, class = void>
struct has_record : std::false_type {};
template <class T>
struct has_record<
    T,
    std::void_t<decltype(std::declval<const T&>()
                             .record(std::declval<const Box&>(), std::declval<Rec&>()))>>
    : std::true_type {};

// Opt-out: elements without record() are described by kind.
template <class T, class = void>
struct has_kind : std::false_type {};
template <class T>
struct has_kind<T, std::void_t<decltype(std::declval<const T&>().kind())>>
    : std::is_same<decltype(std::declval<const T&>().kind()), const char*> {};

// Clip intent; emitter pushes the clip around children, never in self paint.
template <class T, class = void>
struct has_clip_flag : std::false_type {};
template <class T>
struct has_clip_flag<T, std::void_t<decltype(T::kClipsChildren)>>
    : std::bool_constant<T::kClipsChildren> {};

// Ambient layer binding for one paint pass; binding is plain data.
struct LayerSource {
  std::uint32_t id = 0;
  bool foreign = false;
};

// Type-erased paint sink; marks ride every op, pixels ignore them.
struct AnySink {
  void* sink = nullptr;
  void* feed = nullptr;
  LayerSource layer{};
  void (*entryFn)(void*, void*, const Rec::Entry&) = nullptr;
  void (*fillFn)(void*, const Box&, std::uint32_t, const std::string&) = nullptr;
  void (*fillRoundedFn)(void*, const Box&, int, std::uint32_t, const std::string&) = nullptr;
  void (*fillGradientFn)(void*, const Box&, int, std::uint32_t, std::uint32_t, const std::string&) =
      nullptr;
  void (*textFn)(void*, float, float, const std::string&, std::uint32_t, float, const std::string&) =
      nullptr;
  void (*textStyledFn)(void*, float, float, const std::string&, std::uint32_t, float, float, float, const std::string&) =
      nullptr;
  void (*imageFn)(void*, const Box&, std::uint32_t, bool, const std::string&) = nullptr;
  void (*pushGlassFn)(void*, const Box&, int, bool, std::uint8_t, std::uint32_t, std::uint8_t) =
      nullptr;
  void (*pushClipFn)(void*, const Box&) = nullptr;
  void (*pushBlendFn)(void*, BlendMode) = nullptr;
  void (*pushTransformFn)(void*, float, float) = nullptr;
  void (*pushRotateFn)(void*, float, float, float) = nullptr;
  void (*fillRadialFn)(void*, const Box&, int, std::uint32_t, std::uint32_t, const std::string&) =
      nullptr;
  void (*fillChamferFn)(void*, const Box&, float, std::uint32_t, const std::string&) = nullptr;
  void (*strokeChamferFn)(void*, const Box&, float, float, std::uint32_t, const std::string&) =
      nullptr;
  void (*arcFn)(void*, float, float, float, float, float, float, std::uint32_t, const std::string&) =
      nullptr;
  void (*polylineFn)(void*, const float*, std::size_t, float, float, std::uint32_t, const std::string&) =
      nullptr;
  void (*pushShadowFn)(void*, float, float, float, std::uint32_t, float, float) = nullptr;
  void (*pushEffectFn)(void*, const Box&, int, std::uint32_t, std::uint32_t, float) = nullptr;
  void (*popFn)(void*) = nullptr;
  void (*slotFn)(void*, const Box&, std::uint32_t) = nullptr;
  void (*strokeFn)(void*, const Box&, int, float, std::uint32_t, const std::string&) = nullptr;
  void entry(const Rec::Entry& en) { entryFn(sink, feed, en); }
  void fill(const Box& b, std::uint32_t rgba, const std::string& mark = {}) {
    fillFn(sink, b, rgba, mark);
  }
  void fillRounded(const Box& b, int radius, std::uint32_t rgba, const std::string& mark = {}) {
    fillRoundedFn(sink, b, radius, rgba, mark);
  }
  void fillGradient(
      const Box& b,
      int radius,
      std::uint32_t top,
      std::uint32_t bottom,
      const std::string& mark = {}
  ) {
    fillGradientFn(sink, b, radius, top, bottom, mark);
  }
  void fillRadial(
      const Box& b,
      int radius,
      std::uint32_t center,
      std::uint32_t edge,
      const std::string& mark = {}
  ) {
    fillRadialFn(sink, b, radius, center, edge, mark);
  }
  void fillChamfer(const Box& b, float cut, std::uint32_t rgba, const std::string& mark = {}) {
    fillChamferFn(sink, b, cut, rgba, mark);
  }
  void strokeChamfer(
      const Box& b, float cut, float width, std::uint32_t rgba, const std::string& mark = {}
  ) {
    strokeChamferFn(sink, b, cut, width, rgba, mark);
  }
  void arc(
      float cx,
      float cy,
      float radius,
      float thickness,
      float startDeg,
      float sweepDeg,
      std::uint32_t rgba,
      const std::string& mark = {}
  ) {
    arcFn(sink, cx, cy, radius, thickness, startDeg, sweepDeg, rgba, mark);
  }
  void polyline(
      const float* xy,
      std::size_t nPoints,
      float width,
      float dotPitch,
      std::uint32_t rgba,
      const std::string& mark = {}
  ) {
    polylineFn(sink, xy, nPoints, width, dotPitch, rgba, mark);
  }
  // `opacity` fades the caster, not the shadow. 0 casts a plate that must
  // not paint (the visible card is a sibling glass group).
  void pushShadow(
      float sigma, float ox, float oy, std::uint32_t color, float expand, float opacity = 1.f
  ) {
    pushShadowFn(sink, sigma, ox, oy, color, expand, opacity);
  }
  void pushEffect(
      const Box& b, int radius, std::uint32_t kind, std::uint32_t color, float intensity
  ) {
    pushEffectFn(sink, b, radius, kind, color, intensity);
  }
  void text(
      float x,
      float y,
      const std::string& s,
      std::uint32_t rgba,
      float size,
      const std::string& mark = {}
  ) {
    textFn(sink, x, y, s, rgba, size, mark);
  }
  void textStyled(
      float x,
      float y,
      const std::string& s,
      std::uint32_t rgba,
      float size,
      float condense,
      float weight,
      const std::string& mark = {}
  ) {
    if (textStyledFn) {
      textStyledFn(sink, x, y, s, rgba, size, condense, weight, mark);
      return;
    }
    textFn(sink, x, y, s, rgba, size, mark);
  }
  void image(const Box& b, std::uint32_t id, bool foreign, const std::string& mark = {}) {
    imageFn(sink, b, id, foreign, mark);
  }
  void pushGlass(
      const Box& b,
      int radius,
      bool clear,
      std::uint8_t edge,
      std::uint32_t tint,
      std::uint8_t material = 0
  ) {
    pushGlassFn(sink, b, radius, clear, edge, tint, material);
  }
  void pushClip(const Box& b) { pushClipFn(sink, b); }
  void pushBlend(BlendMode m) { pushBlendFn(sink, m); }
  void pushTransform(float dx, float dy) { pushTransformFn(sink, dx, dy); }
  void pushRotate(float angleDeg, float cx, float cy) { pushRotateFn(sink, angleDeg, cx, cy); }
  void pop() { popFn(sink); }
  void slot(const Box& b, std::uint32_t id) { slotFn(sink, b, id); }
  void stroke(
      const Box& b, int radius, float width, std::uint32_t rgba, const std::string& mark = {}
  ) {
    strokeFn(sink, b, radius, width, rgba, mark);
  }
};

// The one table from a concrete sink's methods to AnySink's ops; entry
// and feed are bound by the caller.
template <class S>
void bindOps(AnySink& a) {
  a.fillFn = [](void* s, const Box& b, std::uint32_t rgba, const std::string& mark) {
    static_cast<S*>(s)->fill(b, rgba, mark);
  };
  a.fillRoundedFn = [](void* s, const Box& b, int radius, std::uint32_t rgba, const std::string& m
                    ) { static_cast<S*>(s)->fillRounded(b, radius, rgba, m); };
  a.fillGradientFn = [](void* s,
                        const Box& b,
                        int radius,
                        std::uint32_t top,
                        std::uint32_t bottom,
                        const std::string& m) {
    static_cast<S*>(s)->fillGradient(b, radius, top, bottom, m);
  };
  a.textFn = [](void* s,
                float x,
                float y,
                const std::string& str,
                std::uint32_t rgba,
                float size,
                const std::string& m) { static_cast<S*>(s)->text(x, y, str, rgba, size, m); };
  a.textStyledFn = [](void* s,
                      float x,
                      float y,
                      const std::string& str,
                      std::uint32_t rgba,
                      float size,
                      float condense,
                      float weight,
                      const std::string& m) {
    static_cast<S*>(s)->textStyled(x, y, str, rgba, size, condense, weight, m);
  };
  a.imageFn = [](void* s, const Box& b, std::uint32_t id, bool foreign, const std::string& m) {
    static_cast<S*>(s)->image(b, id, foreign, m);
  };
  a.pushGlassFn = [](void* s,
                     const Box& b,
                     int radius,
                     bool clear,
                     std::uint8_t edge,
                     std::uint32_t tint,
                     std::uint8_t material) {
    static_cast<S*>(s)->pushGlass(b, radius, clear, edge, tint, material);
  };
  a.pushClipFn = [](void* s, const Box& b) { static_cast<S*>(s)->pushClip(b); };
  a.pushBlendFn = [](void* s, BlendMode m) { static_cast<S*>(s)->pushBlend(m); };
  a.pushTransformFn = [](void* s, float dx, float dy) {
    static_cast<S*>(s)->pushTransform(dx, dy);
  };
  a.pushRotateFn = [](void* s, float angleDeg, float cx, float cy) {
    static_cast<S*>(s)->pushRotate(angleDeg, cx, cy);
  };
  a.fillRadialFn = [](void* s,
                      const Box& b,
                      int radius,
                      std::uint32_t center,
                      std::uint32_t edge,
                      const std::string& m) {
    static_cast<S*>(s)->fillRadial(b, radius, center, edge, m);
  };
  a.fillChamferFn = [](void* s, const Box& b, float cut, std::uint32_t rgba, const std::string& m) {
    static_cast<S*>(s)->fillChamfer(b, cut, rgba, m);
  };
  a.strokeChamferFn =
      [](void* s, const Box& b, float cut, float width, std::uint32_t rgba, const std::string& m) {
        static_cast<S*>(s)->strokeChamfer(b, cut, width, rgba, m);
      };
  a.arcFn = [](void* s,
               float cx,
               float cy,
               float radius,
               float thickness,
               float startDeg,
               float sweepDeg,
               std::uint32_t rgba,
               const std::string& m) {
    static_cast<S*>(s)->arc(cx, cy, radius, thickness, startDeg, sweepDeg, rgba, m);
  };
  a.polylineFn = [](void* s,
                    const float* xy,
                    std::size_t n,
                    float width,
                    float dotPitch,
                    std::uint32_t rgba,
                    const std::string& m) {
    static_cast<S*>(s)->polyline(xy, n, width, dotPitch, rgba, m);
  };
  a.pushShadowFn =
      [](void* s, float sigma, float ox, float oy, std::uint32_t color, float expand, float opacity
      ) { static_cast<S*>(s)->pushShadow(sigma, ox, oy, color, expand, opacity); };
  a.pushEffectFn =
      [](void* s, const Box& b, int radius, std::uint32_t kind, std::uint32_t color, float intensity
      ) { static_cast<S*>(s)->pushEffect(b, radius, kind, color, intensity); };
  a.popFn = [](void* s) { static_cast<S*>(s)->pop(); };
  a.slotFn = [](void* s, const Box& b, std::uint32_t id) { static_cast<S*>(s)->slot(b, id); };
  a.strokeFn =
      [](void* s, const Box& b, int radius, float width, std::uint32_t rgba, const std::string& m) {
        static_cast<S*>(s)->stroke(b, radius, width, rgba, m);
      };
}

template <class S, class F>
AnySink wrapSink(S& sink, F& feed) {
  AnySink a;
  a.sink = &sink;
  a.feed = &feed;
  a.layer = LayerSource{feed.id, feed.foreign};
  a.entryFn = [](void* s, void* f, const Rec::Entry& en) {
    static_cast<S*>(s)->entry(en, *static_cast<F*>(f));
  };
  bindOps<S>(a);
  return a;
}

template <class T, class = void>
struct has_paint : std::false_type {};
template <class T>
struct has_paint<
    T,
    std::void_t<decltype(std::declval<const T&>().paint(
        std::declval<const Box&>(), std::declval<AnySink&>(), std::declval<const Engine&>()
    ))>> : std::true_type {};

template <class T>
struct is_tagged : std::false_type {};

// Optional runtime init: toolkit calls attach() on first place/paint
// with the walk Engine when the element defines it.
template <class T, class = void>
struct has_attach : std::false_type {};
template <class T>
struct has_attach<
    T,
    std::void_t<decltype(std::declval<const T&>().attach(std::declval<const Engine&>()))>>
    : std::true_type {};

// Borrowed a11y id; tree must outlive the pointer.
template <class T>
std::optional<std::reference_wrapper<const std::string>> elementSemanticPtr(const T& x) {
  if constexpr (is_tagged<T>::value) {
    if constexpr (std::is_same<typename T::tag_type, SemanticTag>::value) {
      if (x.tag_.id.empty()) return std::nullopt;
      return std::cref(x.tag_.id);
    } else {
      return elementSemanticPtr(x.inner_);
    }
  } else if constexpr (has_semantic<T>::value) {
    if (x.semantic.empty()) return std::nullopt;
    return std::cref(x.semantic);
  } else {
    return std::nullopt;
  }
}

// Child transparency: child refs valid only during the visit.
template <class T, class = void>
struct has_eachChild : std::false_type {};
template <class T>
struct has_eachChild<
    T,
    std::void_t<decltype(std::declval<const T&>().eachChild(std::declval<void (*)(const Block&)>())
    )>> : std::true_type {};

template <class T, class = void>
struct has_child_vector : std::false_type {};
template <class T>
struct has_child_vector<
    T,
    std::void_t<
        decltype(std::declval<const T&>().children.begin()),
        decltype(std::declval<const T&>().children.end())>> : std::true_type {};

template <class T, class = void>
struct has_childBox : std::false_type {};
template <class T>
struct has_childBox<
    T,
    std::void_t<decltype(std::declval<const T&>().childBox(
        std::declval<std::size_t>(), std::declval<const Box&>(), std::declval<const Engine&>()
    ))>>
    : std::is_same<
          decltype(std::declval<const T&>().childBox(
              std::declval<std::size_t>(), std::declval<const Box&>(), std::declval<const Engine&>()
          )),
          Box> {};

template <class T>
struct has_children
    : std::bool_constant<
          (has_eachChild<T>::value || has_child_vector<T>::value) && has_childBox<T>::value> {};

template <class T, class F>
void eachChildOf(const T& el, F&& f) {
  static_assert(has_children<T>::value, "element has no traversable children");
  if constexpr (has_eachChild<T>::value) {
    el.eachChild(std::forward<F>(f));
  } else {
    for (const auto& c : el.children) f(c);
  }
}

template <class T, class = void>
struct has_inner_box : std::false_type {};
template <class T>
struct has_inner_box<
    T,
    std::void_t<decltype(std::declval<const T&>().innerBox(std::declval<const Box&>()))>>
    : std::is_same<decltype(std::declval<const T&>().innerBox(std::declval<const Box&>())), Box> {};

struct Custom {
  static constexpr const char* kKind = "Custom";
  const char* kind() const { return kKind; }
  struct VTable {
    const std::type_info& (*type)();
    Box (*place)(const void* s, bool heap, Constraints c, const Engine& e);
    void (*record)(const void* s, bool heap, const Box& b, Rec& r);
    bool (*handle)(const void* s, bool heap, const Event& ev);
    bool (*handleAt)(const void* s, bool heap, const Box& b, const Event& ev);
    void (*paint)(const void* s, bool heap, const Box& b, AnySink& sink, const Engine& e);
    bool (*clips)(const void* s, bool heap);
    std::optional<std::reference_wrapper<const std::string>> (*semantic)(const void* s, bool heap);
    ThemeScope (*themeScope)(const void* s, bool heap);
    std::size_t (*childCount)(const void* s, bool heap);
    const Block* (*childAt)(const void* s, bool heap, std::size_t i);
    Box (*childBox)(const void* s, bool heap, std::size_t i, const Box& b, const Engine& e);
    void (*destroy)(void* s, bool heap);
    void (*moveTo)(void* d, void* s, bool& dheap, bool& sheap);
    void (*cloneTo)(const void* s, bool heap, void* d, bool& dheap);
    bool (*hasTag)(const void* s, bool heap, const std::type_info& t);
    const void* (*tagPtr)(const void* s, bool heap, const std::type_info& t);
  };

  static constexpr std::size_t kCapacity = 64;
  alignas(std::max_align_t) unsigned char buf[kCapacity];
  const VTable* vt = nullptr;
  bool onHeap = false;

  Custom() = default;
  template <class T, class = std::enable_if_t<!std::is_same<std::decay_t<T>, Custom>::value>>
  Custom(T&& t) {
    using U = std::decay_t<T>;
    static_assert(
        std::is_same<
            decltype(std::declval<const U&>()
                         .place(std::declval<Constraints>(), std::declval<const Engine&>())),
            Box>::value,
        "custom element must provide 'Box place(Constraints, const Engine&) const' "
        "(see elements/Row.h)"
    );
    static_assert(
        has_record<U>::value || has_kind<U>::value,
        "custom element must provide 'void record(const Box&, Rec&) const' "
        "or 'const char* kind() const' (see elements/Row.h)"
    );
    vt = vtableFor<U>();
    if (sizeof(U) <= kCapacity) {
      new (buf) U(std::forward<T>(t));
      onHeap = false;
    } else {
      *reinterpret_cast<U**>(buf) = new U(std::forward<T>(t));
      onHeap = true;
    }
  }
  Custom(const Custom& o) : vt(o.vt), onHeap(false) {
    if (vt) {
      bool dheap = false;
      vt->cloneTo(o.buf, o.onHeap, buf, dheap);
      onHeap = dheap;
    }
  }
  Custom(Custom&& o) noexcept : vt(o.vt), onHeap(false) {
    if (vt) {
      vt->moveTo(buf, o.buf, onHeap, o.onHeap);
      o.vt = nullptr;
    }
  }
  Custom& operator=(const Custom& o) {
    if (this != &o) {
      reset();
      vt = o.vt;
      if (vt) {
        bool dheap = false;
        vt->cloneTo(o.buf, o.onHeap, buf, dheap);
        onHeap = dheap;
      }
    }
    return *this;
  }
  Custom& operator=(Custom&& o) noexcept {
    if (this != &o) {
      reset();
      vt = o.vt;
      if (vt) {
        vt->moveTo(buf, o.buf, onHeap, o.onHeap);
        o.vt = nullptr;
      }
    }
    return *this;
  }
  ~Custom() { reset(); }
  void reset() {
    if (vt) {
      vt->destroy(buf, onHeap);
      vt = nullptr;
      onHeap = false;
    }
  }
  bool empty() const { return !vt; }
  bool handle(const Event& ev) const {
    return vt && vt->handle ? vt->handle(buf, onHeap, ev) : false;
  }
  // Prefers handleAt, falls back to handle.
  bool handleAt(const Box& b, const Event& ev) const {
    if (vt && vt->handleAt) return vt->handleAt(buf, onHeap, b, ev);
    return handle(ev);
  }
  bool handles() const { return vt && (vt->handle != nullptr || vt->handleAt != nullptr); }
  bool hasTag(const std::type_info& t) const {
    return vt && vt->hasTag ? vt->hasTag(buf, onHeap, t) : false;
  }
  const void* tagPtr(const std::type_info& t) const {
    return vt && vt->tagPtr ? vt->tagPtr(buf, onHeap, t) : nullptr;
  }
  std::size_t childCount() const { return vt && vt->childCount ? vt->childCount(buf, onHeap) : 0; }
  Box childBox(std::size_t i, const Box& b, const Engine& e) const {
    if (vt && vt->childBox) return vt->childBox(buf, onHeap, i, b, e);
    return b;
  }
  // Single-walk transparency; child refs valid only during cb.
  template <class F>
  void visit(const Box& b, const Engine& e, F&& cb) const {
    if (!vt || !vt->childCount || !vt->childAt || !vt->childBox) return;
    std::size_t n = vt->childCount(buf, onHeap);
    for (std::size_t i = 0; i < n; ++i) {
      const Block* c = vt->childAt(buf, onHeap, i);
      if (!c) continue;
      cb(*c, vt->childBox(buf, onHeap, i, b, e));
    }
  }
  template <class U>
  std::optional<std::reference_wrapper<const U>> as() const {
    if (!vt || vt->type() != typeid(U)) return std::nullopt;
    return std::cref(*static_cast<const U*>(addr<U>(buf, onHeap)));
  }
  Box place(Constraints c, const Engine& e) const {
    if (!vt)
      return {0, 0, detail::clampF(0.0f, c.minW, c.maxW), detail::clampF(0.0f, c.minH, c.maxH)};
    return vt->place(buf, onHeap, c, e);
  }
  void record(const Box& b, Rec& r) const {
    if (!vt) return;
    vt->record(buf, onHeap, b, r);
  }
  bool paints() const { return vt && vt->paint != nullptr; }
  void paint(const Box& b, AnySink& sink, const Engine& e) const {
    if (vt && vt->paint) vt->paint(buf, onHeap, b, sink, e);
  }
  bool clipsChildren() const { return vt && vt->clips ? vt->clips(buf, onHeap) : false; }
  // Borrowed id; tree must outlive the pointer.
  std::optional<std::reference_wrapper<const std::string>> semanticPtr() const {
    if (vt && vt->semantic) return vt->semantic(buf, onHeap);
    return std::nullopt;
  }
  // Borrowed view; tree must outlive it.
  ThemeScope themeScope() const {
    if (vt && vt->themeScope) return vt->themeScope(buf, onHeap);
    return {};
  }

 private:
  template <class U>
  static const void* addr(const void* s, bool heap) {
    return heap ? *static_cast<U* const*>(s) : s;
  }
  template <class U>
  static bool (*handleFor())(const void*, bool, const Event&) {
    if constexpr (has_handle<U>::value) {
      return [](const void* s, bool heap, const Event& ev) -> bool {
        return static_cast<const U*>(addr<U>(s, heap))->handle(ev);
      };
    } else {
      return nullptr;
    }
  }

  template <class U>
  static bool (*handleAtFor())(const void*, bool, const Box&, const Event&) {
    if constexpr (has_handle_at<U>::value) {
      return [](const void* s, bool heap, const Box& b, const Event& ev) -> bool {
        return static_cast<const U*>(addr<U>(s, heap))->handleAt(b, ev);
      };
    } else {
      return nullptr;
    }
  }

  template <class U>
  static void (*paintFor())(const void*, bool, const Box&, AnySink&, const Engine&) {
    if constexpr (has_paint<U>::value) {
      return [](const void* s, bool heap, const Box& b, AnySink& sink, const Engine& e) {
        const U* u = static_cast<const U*>(addr<U>(s, heap));
        if constexpr (has_attach<U>::value) u->attach(e);
        u->paint(b, sink, e);
      };
    } else {
      return nullptr;
    }
  }

  template <class U>
  static std::size_t (*childCountFn())(const void*, bool) {
    if constexpr (has_children<U>::value) {
      return [](const void* s, bool heap) -> std::size_t {
        const U* u = static_cast<const U*>(addr<U>(s, heap));
        if constexpr (has_child_vector<U>::value) {
          return u->children.size();
        } else {
          std::size_t n = 0;
          u->eachChild([&](const Block&) { ++n; });
          return n;
        }
      };
    } else {
      return nullptr;
    }
  }

  template <class U>
  static const Block* (*childAtFn())(const void*, bool, std::size_t) {
    if constexpr (has_children<U>::value) {
      return [](const void* s, bool heap, std::size_t want) -> const Block* {
        const U* u = static_cast<const U*>(addr<U>(s, heap));
        if constexpr (has_child_vector<U>::value) {
          return want < u->children.size() ? &u->children[want] : nullptr;
        } else {
          const Block* found = nullptr;
          std::size_t i = 0;
          u->eachChild([&](const Block& c) {
            if (i++ == want) found = &c;
          });
          return found;
        }
      };
    } else {
      return nullptr;
    }
  }

  template <class U>
  static Box (*childBoxFn())(const void*, bool, std::size_t, const Box&, const Engine&) {
    if constexpr (has_children<U>::value) {
      return [](const void* s, bool heap, std::size_t i, const Box& b, const Engine& e) -> Box {
        return static_cast<const U*>(addr<U>(s, heap))->childBox(i, b, e);
      };
    } else {
      return nullptr;
    }
  }

  template <class U>
  static bool hasTagImpl(const std::type_info& t) {
    if constexpr (is_tagged<U>::value) {
      if (t == typeid(typename U::tag_type)) return true;
      return hasTagImpl<typename U::inner_type>(t);
    } else {
      return false;
    }
  }

  template <class U>
  static const void* tagPtrImpl(const U& u, const std::type_info& t) {
    if constexpr (is_tagged<U>::value) {
      if (t == typeid(typename U::tag_type)) return static_cast<const void*>(&u.tag_);
      return tagPtrImpl(u.inner_, t);
    } else {
      return nullptr;
    }
  }

  template <class U>
  static bool (*hasTagFor())(const void*, bool, const std::type_info&) {
    return [](const void*, bool, const std::type_info& t) -> bool { return hasTagImpl<U>(t); };
  }

  template <class U>
  static const void* (*tagPtrFor())(const void*, bool, const std::type_info&) {
    return [](const void* s, bool heap, const std::type_info& t) -> const void* {
      return tagPtrImpl(*static_cast<const U*>(addr<U>(s, heap)), t);
    };
  }

  template <class U>
  static bool (*clipsFor())(const void*, bool) {
    return [](const void*, bool) -> bool { return has_clip_flag<U>::value; };
  }

  template <class U>
  static std::optional<std::reference_wrapper<const std::string>> (*semanticFor())(
      const void*, bool
  ) {
    return [](const void* s, bool heap) {
      return elementSemanticPtr(*static_cast<const U*>(addr<U>(s, heap)));
    };
  }

  template <class U>
  static ThemeScope (*themeScopeFor())(const void*, bool) {
    return [](const void* s, bool heap) -> ThemeScope {
      if constexpr (has_theme_entries<U>::value) {
        return static_cast<const U*>(addr<U>(s, heap))->themeScope();
      } else {
        return {};
      }
    };
  }

  template <class U>
  static const VTable* vtableFor() {
    static const VTable v{
        []() -> const std::type_info& { return typeid(U); },
        [](const void* s, bool heap, Constraints c, const Engine& e) -> Box {
          const U* u = static_cast<const U*>(addr<U>(s, heap));
          if constexpr (has_attach<U>::value) u->attach(e);
          return u->place(c, e);
        },
        [](const void* s, bool heap, const Box& b, Rec& r) {
          const U* u = static_cast<const U*>(addr<U>(s, heap));
          if constexpr (has_record<U>::value) {
            u->record(b, r);
          } else {
            r.push(u->kind(), b);
          }
        },
        handleFor<U>(),
        handleAtFor<U>(),
        paintFor<U>(),
        clipsFor<U>(),
        semanticFor<U>(),
        themeScopeFor<U>(),
        childCountFn<U>(),
        childAtFn<U>(),
        childBoxFn<U>(),
        [](void* s, bool heap) {
          if (heap) {
            delete *static_cast<U**>(s);
          } else {
            static_cast<U*>(s)->~U();
          }
        },
        [](void* d, void* s, bool& dheap, bool& sheap) {
          if (sheap) {
            *static_cast<U**>(d) = *static_cast<U**>(s);
            dheap = true;
            sheap = false;
          } else {
            new (d) U(std::move(*static_cast<U*>(s)));
            static_cast<U*>(s)->~U();
            dheap = false;
          }
        },
        [](const void* s, bool heap, void* d, bool& dheap) {
          if (heap) {
            *static_cast<U**>(d) = new U(*static_cast<const U*>(*static_cast<U* const*>(s)));
            dheap = true;
          } else {
            new (d) U(*static_cast<const U*>(s));
            dheap = false;
          }
        },
        hasTagFor<U>(),
        tagPtrFor<U>()
    };
    return &v;
  }
};

}  // namespace toolkit
}  // namespace glim
