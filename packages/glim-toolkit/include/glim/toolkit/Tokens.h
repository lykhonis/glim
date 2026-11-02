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
  // Dashboard ramp (reference: near-black neutral, orange accent).
  // Window near-black, cards lifted one step, active chips another.
  static constexpr std::uint32_t kWindowBg = 0x1e1e1eff;
  static constexpr std::uint32_t kCardFill = 0x2c2c2eff;
  static constexpr std::uint32_t kCardStroke = 0xffffff14;
  static constexpr std::uint32_t kButtonFill = 0x3d3d42ff;
  static constexpr std::uint32_t kButtonPrimaryFill = 0xff5a1fff;
  static constexpr std::uint32_t kButtonPrimaryTop = 0xff7a3dff;
  static constexpr std::uint32_t kButtonPrimaryBottom = 0xf43f00ff;
  static constexpr std::uint32_t kAccent = 0xff5a1fff;
  static constexpr std::uint32_t kInkOnLight = 0xffffffff;
  static constexpr std::uint32_t kInkSecondary = 0xa1a1aaff;
  static constexpr std::uint32_t kInkOnDark = 0x1e1e1eff;
  // Hairline dividers: faint white rule over dark chrome.
  static constexpr std::uint32_t kHairline = 0xffffff1f;
  // Display numerals (stat values): exactly 2x body.
  static constexpr float kTypeDisplay = 28.0f;
  static constexpr std::uint32_t kScrim = 0x00000080;
  // Keyboard-only focus ring (focus-visible): orange accent, never
  // painted for pointer (see Bridge.h paintFocusRing).
  static constexpr std::uint32_t kFocusRing = 0xff6a2bff;
  static constexpr std::uint32_t kVideoFallback = 0x2c2c2eff;
  static constexpr float kGlassThickness = 16.0f;
  static constexpr int kCardRadius = 12;
  static constexpr int kButtonRadius = 7;
};

}  // namespace toolkit
}  // namespace glim
