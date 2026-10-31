#pragma once

#include <cstddef>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "glim/toolkit/Event.h"
#include "glim/toolkit/Layout.h"
#include "glim/toolkit/Place.h"

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

template <class T>
struct is_tagged : std::false_type {};

// Opt-in child transparency: an element exposing its composed children
// (eachChild visitor + per-child box in a laid-out parent box) is traversed
// by focus enumeration and hit-testing instead of treated as an opaque leaf.
//
// Contract: eachChild must call the visitor with references to STORED child
// Blocks owned by the element (e.g. elements of a `children` vector) — never
// temporaries. The engine holds the resulting references only for the
// duration of the visitor callback (see `visit`), but `childAt` callers must
// use the pointer immediately before any mutation; synthesizing children on
// the fly dangles. Containers are small; count+index is O(n^2) worst case.
template <class T, class = void>
struct has_children : std::false_type {};
template <class T>
struct has_children<
    T,
    std::void_t<
        decltype(std::declval<const T&>().eachChild(std::declval<void (*)(const Block&)>())),
        decltype(std::declval<const T&>().childBox(
            std::declval<std::size_t>(), std::declval<const Box&>(), std::declval<const Engine&>()
        ))>> : std::true_type {};

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
    void (*record)(const void* s, bool heap, const Box& b, Rec& r, const Engine& e);
    bool (*handle)(const void* s, bool heap, const Event& ev);
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
        std::is_same<
            decltype(std::declval<const U&>().record(
                std::declval<const Box&>(), std::declval<Rec&>(), std::declval<const Engine&>()
            )),
            void>::value,
        "custom element must provide 'void record(const Box&, Rec&, const Engine&) "
        "const' (see elements/Row.h)"
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
  bool handles() const { return vt && vt->handle != nullptr; }
  bool hasTag(const std::type_info& t) const {
    return vt && vt->hasTag ? vt->hasTag(buf, onHeap, t) : false;
  }
  const void* tagPtr(const std::type_info& t) const {
    return vt && vt->tagPtr ? vt->tagPtr(buf, onHeap, t) : nullptr;
  }
  std::size_t childCount() const { return vt && vt->childCount ? vt->childCount(buf, onHeap) : 0; }
  const Block* childAt(std::size_t i) const {
    return vt && vt->childAt ? vt->childAt(buf, onHeap, i) : nullptr;
  }
  Box childBox(std::size_t i, const Box& b, const Engine& e) const {
    if (vt && vt->childBox) return vt->childBox(buf, onHeap, i, b, e);
    return b;
  }
  // Single-walk transparency: invokes cb(child, childBox) for each stored
  // child without exposing raw pointers. Prefer over childCount/childAt loops
  // (one walk, no dangling window beyond the callback).
  // Lifetime: child references are valid only during cb.
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
  const U* as() const {
    if (!vt || vt->type() != typeid(U)) return nullptr;
    return static_cast<const U*>(addr<U>(buf, onHeap));
  }
  Box place(Constraints c, const Engine& e) const {
    if (!vt)
      return {0, 0, detail::clampF(0.0f, c.minW, c.maxW), detail::clampF(0.0f, c.minH, c.maxH)};
    return vt->place(buf, onHeap, c, e);
  }
  void record(const Box& b, Rec& r, const Engine& e) const {
    if (!vt) return;
    vt->record(buf, onHeap, b, r, e);
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
  static std::size_t (*childCountFn())(const void*, bool) {
    if constexpr (has_children<U>::value) {
      return [](const void* s, bool heap) -> std::size_t {
        std::size_t n = 0;
        static_cast<const U*>(addr<U>(s, heap))->eachChild([&](const Block&) { ++n; });
        return n;
      };
    } else {
      return nullptr;
    }
  }

  template <class U>
  static const Block* (*childAtFn())(const void*, bool, std::size_t) {
    if constexpr (has_children<U>::value) {
      return [](const void* s, bool heap, std::size_t want) -> const Block* {
        const Block* found = nullptr;
        std::size_t i = 0;
        static_cast<const U*>(addr<U>(s, heap))->eachChild([&](const Block& c) {
          if (i++ == want) found = &c;
        });
        return found;
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
  static const VTable* vtableFor() {
    static const VTable v{
        []() -> const std::type_info& { return typeid(U); },
        [](const void* s, bool heap, Constraints c, const Engine& e) -> Box {
          return static_cast<const U*>(addr<U>(s, heap))->place(c, e);
        },
        [](const void* s, bool heap, const Box& b, Rec& r, const Engine& e) {
          static_cast<const U*>(addr<U>(s, heap))->record(b, r, e);
        },
        handleFor<U>(),
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
