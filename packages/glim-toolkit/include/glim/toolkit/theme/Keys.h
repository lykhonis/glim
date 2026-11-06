#pragma once

// Dynamic theme keys: app keys via themeKey; builtins cover the current ramp.
#include <bit>
#include <cstddef>
#include <cstdint>
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

// App-defined key: constexpr-hashed, high bit keeps customs out of the builtin range.
inline constexpr ThemeKey themeKey(const char* name) {
  return ThemeKey{detail::themeFNV1a(name) | 0x80000000u};
}

// Builtin keys for toolkit-owned surfaces; everything else is user space.
inline constexpr ThemeKey kThemeWindowBg{1};
inline constexpr ThemeKey kThemeInkOnLight{8};
inline constexpr ThemeKey kThemeFocusRing{11};
inline constexpr ThemeKey kThemeTypeLine{13};

struct ThemeEntry {
  ThemeKey key;
  std::uint32_t bits = 0;
};

// Borrowed entries from the owning node; inner scopes shadow outer ones.
inline constexpr std::size_t kMaxThemeEntries = 8;

struct ThemeScope {
  ThemeEntry entries[kMaxThemeEntries]{};
  std::size_t count = 0;
};

inline ThemeEntry themeColor(ThemeKey key, std::uint32_t rgba) { return {key, rgba}; }
inline ThemeEntry themeFloat(ThemeKey key, float v) {
  return {key, std::bit_cast<std::uint32_t>(v)};
}
inline ThemeEntry themeInt(ThemeKey key, int v) { return {key, static_cast<std::uint32_t>(v)}; }

inline std::uint32_t themeColor(const Engine& e, ThemeKey key, std::uint32_t fallback) {
  for (auto p = e.env; p; p = p->get().parent) {
    const Env& n = p->get();
    if (n.type && *n.type == typeid(ThemeScope)) {
      const auto* s = static_cast<const ThemeScope*>(n.ptr);
      for (std::size_t i = s->count; i-- > 0;) {
        if (s->entries[i].key == key) return s->entries[i].bits;
      }
    }
  }
  return fallback;
}

inline float themeFloat(const Engine& e, ThemeKey key, float fallback) {
  for (auto p = e.env; p; p = p->get().parent) {
    const Env& n = p->get();
    if (n.type && *n.type == typeid(ThemeScope)) {
      const auto* s = static_cast<const ThemeScope*>(n.ptr);
      for (std::size_t i = s->count; i-- > 0;) {
        if (s->entries[i].key == key) {
          return std::bit_cast<float>(s->entries[i].bits);
        }
      }
    }
  }
  return fallback;
}

inline int themeInt(const Engine& e, ThemeKey key, int fallback) {
  for (auto p = e.env; p; p = p->get().parent) {
    const Env& n = p->get();
    if (n.type && *n.type == typeid(ThemeScope)) {
      const auto* s = static_cast<const ThemeScope*>(n.ptr);
      for (std::size_t i = s->count; i-- > 0;) {
        if (s->entries[i].key == key) return static_cast<int>(s->entries[i].bits);
      }
    }
  }
  return fallback;
}

// Opt-in scope: pushed by walk and by place/childBox so layout and paint agree.
template <class T, class = void>
struct has_theme_entries : std::false_type {};
template <class T>
struct has_theme_entries<T, std::void_t<decltype(std::declval<const T&>().themeScope())>>
    : std::is_same<decltype(std::declval<const T&>().themeScope()), ThemeScope> {};

}  // namespace toolkit
}  // namespace glim
