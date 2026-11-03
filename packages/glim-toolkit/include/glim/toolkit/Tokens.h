#pragma once

#include <cstdint>

namespace glim {
namespace toolkit {

// Toolkit-owned constants only: text metrics, the host window fill,
// the focus-ring ink, the glass default radius, the gap clamp, and
// the physical glass thickness. Widget ramps (cards, buttons, gel,
// thumbs, tracks, accents) are user space — see the app's own theme
// header (showcase: widgets/ShowcaseTheme.h).
struct Tokens {
  static constexpr float kTypeAdvance = 8.0f;
  static constexpr float kTypeLine = 14.0f;
  static constexpr int kGapMax = 24;
  static constexpr std::uint32_t kWindowBg = 0x1e1e1eff;
  static constexpr std::uint32_t kInkOnLight = 0xffffffff;
  // Keyboard-only focus ring (focus-visible): orange accent, never
  // painted for pointer (see Emit.h paintFocusRing).
  static constexpr std::uint32_t kFocusRing = 0xff6a2bff;
  static constexpr float kGlassThickness = 16.0f;
  static constexpr int kGlassRadius = 12;
};

}  // namespace toolkit
}  // namespace glim
