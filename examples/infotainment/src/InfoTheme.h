#pragma once

// Infotainment theme (user space): dark sci-fi cockpit + orange accent.
// Two designed values: window/ink/cards + orange ramp. Widgets read
// keys, fall back to Info::* constants so headless paint is exact.

#include <cstdint>

#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

inline constexpr ThemeKey kInfoCardTop{themeKey("info.cardTop")};
inline constexpr ThemeKey kInfoCardBottom{themeKey("info.cardBottom")};
inline constexpr ThemeKey kInfoAccent{themeKey("info.accent")};

struct Info {
  static constexpr std::uint32_t kWindowBg = 0x05080dff;
  static constexpr std::uint32_t kInk = 0xe8eef5ff;
  static constexpr std::uint32_t kCardTop = 0x0a0f16ff;
  static constexpr std::uint32_t kCardBottom = 0x070b12ff;
  // Orange accent: high contrast on dark cockpit.
  static constexpr std::uint32_t kAccent = 0xff6a00ff;
};

// Alpha-scale a packed RGBA color. Endpoints exact; mid-flight rounding
// (±1) never touches a rest state — callers skip the layer at 0.
inline std::uint32_t withAlpha(std::uint32_t rgba, float a) {
  if (a >= 1.0f) return rgba;
  if (a <= 0.0f) return rgba & 0xffffff00u;
  const unsigned alpha =
      static_cast<unsigned>((rgba & 0xffu) * a + 0.5f);
  return (rgba & 0xffffff00u) | (alpha & 0xffu);
}

inline Block infoThemed(Block child) {
  return themed(
      themeColor(kThemeWindowBg, Info::kWindowBg),
      themeColor(kThemeInkOnLight, Info::kInk),
      themeColor(kInfoCardTop, Info::kCardTop),
      themeColor(kInfoCardBottom, Info::kCardBottom),
      themeColor(kInfoAccent, Info::kAccent),
      std::move(child)
  );
}

}  // namespace toolkit
}  // namespace glim
