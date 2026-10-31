#pragma once

#include <cstdint>

namespace glim {
namespace toolkit {

// Rung 1: named constants only. No tags index into Tokens yet; factories and
// the text measurer reference these instead of repeating literals, so the
// ramp has one home when tags arrive.
//
// Rung 2 (T4 visuals): flat colors + glass params for the paint bridge.
// Per-element tint tags wait for a screen that proves them.
struct Tokens {
  static constexpr float kTypeAdvance = 8.0f;
  static constexpr float kTypeLine = 14.0f;
  static constexpr int kPadCard = 12;
  static constexpr int kButtonPadH = 10;
  static constexpr int kButtonPadV = 7;
  static constexpr int kGapMax = 24;
  static constexpr std::uint32_t kWindowBg = 0x1a2228ff;
  static constexpr std::uint32_t kCardFill = 0x1d2a2aff;
  static constexpr std::uint32_t kButtonFill = 0x2a9d8fff;
  static constexpr std::uint32_t kButtonPrimaryFill = 0xe9c46aff;
  static constexpr std::uint32_t kInkOnLight = 0xf8fafcff;
  static constexpr std::uint32_t kInkOnDark = 0x1a2228ff;
  static constexpr std::uint32_t kScrim = 0x00000080;
  static constexpr std::uint32_t kFocusRing = 0x2a9d8fff;
  static constexpr std::uint32_t kVideoFallback = 0x3d5a80ff;
  static constexpr float kGlassThickness = 16.0f;
  static constexpr int kCardRadius = 12;
  static constexpr int kButtonRadius = 7;
};

}  // namespace toolkit
}  // namespace glim
