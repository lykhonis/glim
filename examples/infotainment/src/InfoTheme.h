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
