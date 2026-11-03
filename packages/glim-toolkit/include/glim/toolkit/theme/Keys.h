#pragma once

// Dynamic theme keys: the hardcoded Theme struct (card, button, ...)
// does not scale — every surface needs a struct edit plus a merge
// edit. Keys extend without touching core: builtins cover the current
// ramp, apps add keys via themeKey("app.surface") (constexpr FNV-1a,
// high-bit range so builtins never collide). No registry.
//
// Values are one uniform 32-bit payload (color stored directly,
// float/int via bit-cast helpers) so inherit/merge/copy never care
// about types; typed accessors decode at the call site with a Tokens
// fallback.
//
// Inherit/merge is structural, not a copy: each themed scope pushes
// one Env node holding its entries; lookup walks the chain
// innermost-out, first hit wins. Inherit = push a node (O(1), zero
// copy of the base). Merge = chained lookup (O(depth x patchSize),
// both tiny). Copy = memcpy of a small inline array. Everything
// immutable; no allocation on the lookup path — near zero-cost.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

struct ThemeKey {
  std::uint32_t id = 0;
  constexpr bool operator==(ThemeKey o) const { return id == o.id; }
  constexpr bool operator!=(ThemeKey o) const { return id != o.id; }
};

namespace detail {

inline constexpr std::uint32_t themeFNV1a(const char* s) {
  std::uint32_t h = 2166136261u;
  while (*s) {
    h ^= static_cast<std::uint32_t>(*s++);
    h *= 16777619u;
  }
  return h;
}

}  // namespace detail

// App-defined key. Constexpr-hashed; the high bit keeps customs out of
// the builtin range (builtins are small ids, high bit clear).
inline constexpr ThemeKey themeKey(const char* name) {
  return ThemeKey{detail::themeFNV1a(name) | 0x80000000u};
}

// Builtin ramp keys for toolkit-owned surfaces: the host window,
// the Lines text ink/size, and the focus ring. Everything else is
// user space — apps add keys via themeKey("app.surface") (constexpr
// FNV-1a, high-bit range so builtins never collide). No registry.
inline constexpr ThemeKey kThemeWindowBg{1};
inline constexpr ThemeKey kThemeInkOnLight{8};
inline constexpr ThemeKey kThemeFocusRing{11};
inline constexpr ThemeKey kThemeTypeLine{13};

struct ThemeEntry {
  ThemeKey key;
  std::uint32_t bits = 0;
};

// A pushed scope: entries borrowed from the owning tree node (never
// owned here). Within one scope later entries shadow earlier ones;
// inner scopes shadow outer ones by chain order.
struct ThemeScope {
  const ThemeEntry* entries = nullptr;
  std::size_t count = 0;
};

inline ThemeEntry themeColor(ThemeKey key, std::uint32_t rgba) { return {key, rgba}; }
inline ThemeEntry themeFloat(ThemeKey key, float v) {
  std::uint32_t b = 0;
  std::memcpy(&b, &v, sizeof(b));
  return {key, b};
}
inline ThemeEntry themeInt(ThemeKey key, int v) { return {key, static_cast<std::uint32_t>(v)}; }

inline std::uint32_t themeColor(const Engine& e, ThemeKey key, std::uint32_t fallback) {
  for (const Env* p = e.env; p; p = p->parent) {
    if (p->type && *p->type == typeid(ThemeScope)) {
      const auto* s = static_cast<const ThemeScope*>(p->ptr);
      for (std::size_t i = s->count; i-- > 0;) {
        if (s->entries[i].key == key) return s->entries[i].bits;
      }
    }
  }
  return fallback;
}

inline float themeFloat(const Engine& e, ThemeKey key, float fallback) {
  for (const Env* p = e.env; p; p = p->parent) {
    if (p->type && *p->type == typeid(ThemeScope)) {
      const auto* s = static_cast<const ThemeScope*>(p->ptr);
      for (std::size_t i = s->count; i-- > 0;) {
        if (s->entries[i].key == key) {
          float v = 0;
          std::uint32_t b = s->entries[i].bits;
          std::memcpy(&v, &b, sizeof(v));
          return v;
        }
      }
    }
  }
  return fallback;
}

inline int themeInt(const Engine& e, ThemeKey key, int fallback) {
  for (const Env* p = e.env; p; p = p->parent) {
    if (p->type && *p->type == typeid(ThemeScope)) {
      const auto* s = static_cast<const ThemeScope*>(p->ptr);
      for (std::size_t i = s->count; i-- > 0;) {
        if (s->entries[i].key == key) return static_cast<int>(s->entries[i].bits);
      }
    }
  }
  return fallback;
}

// Opt-in theme scope: an element exposing its entries is pushed by
// the walk (see Walk.h) and by its own place/childBox, so layout and
// paint agree from one call each.
template <class T, class = void>
struct has_theme_entries : std::false_type {};
template <class T>
struct has_theme_entries<T, std::void_t<decltype(std::declval<const T&>().themeScope())>>
    : std::is_same<decltype(std::declval<const T&>().themeScope()), ThemeScope> {};

}  // namespace toolkit
}  // namespace glim
