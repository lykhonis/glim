#pragma once

#include <cstdint>

namespace glim {
namespace toolkit {

// Toolkit-owned constants only; widget ramps are user space.
struct Tokens {
  static constexpr float kTypeAdvance = 8.0f;
  static constexpr float kTypeLine = 14.0f;
  static constexpr int kGapMax = 24;
  static constexpr std::uint32_t kWindowBg = 0x1e1e1eff;
  static constexpr std::uint32_t kInkOnLight = 0xffffffff;
  // Keyboard-only focus ring (focus-visible), never painted for pointer.
  static constexpr std::uint32_t kFocusRing = 0xff6a2bff;
  static constexpr float kGlassThickness = 16.0f;
  static constexpr int kGlassRadius = 12;
};

}  // namespace toolkit
}  // namespace glim
