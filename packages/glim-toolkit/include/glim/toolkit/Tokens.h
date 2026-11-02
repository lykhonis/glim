#pragma once

#include <cstdint>

namespace glim {
namespace toolkit {

// Named constants only: one home for the ramp.
struct Tokens {
  static constexpr float kTypeAdvance = 8.0f;
  static constexpr float kTypeLine = 14.0f;
  static constexpr int kPadCard = 12;
  static constexpr int kButtonPadH = 10;
  static constexpr int kButtonPadV = 7;
  static constexpr int kGapMax = 24;
  // Dark frost ramp, orange accent.
  static constexpr std::uint32_t kWindowBg = 0x1e1e1eff;
  static constexpr std::uint32_t kCardFill = 0x2c2c2eff;
  static constexpr std::uint32_t kCardStroke = 0xffffff29;
  static constexpr std::uint32_t kButtonFill = 0x3d3d42ff;
  static constexpr std::uint32_t kButtonPrimaryFill = 0xff5a1fff;
  static constexpr std::uint32_t kButtonPrimaryTop = 0xff7a3dff;
  static constexpr std::uint32_t kButtonPrimaryBottom = 0xf43f00ff;
  static constexpr std::uint32_t kAccent = 0xff5a1fff;
  static constexpr std::uint32_t kInkOnLight = 0xffffffff;
  static constexpr std::uint32_t kInkSecondary = 0xc9c9d4ff;
  static constexpr std::uint32_t kInkOnDark = 0x1e1e1eff;
  // Hairline dividers: faint white rule over dark chrome.
  static constexpr std::uint32_t kHairline = 0xffffff1f;
  // Display numerals (stat values): exactly 2x body.
  static constexpr float kTypeDisplay = 28.0f;
  static constexpr std::uint32_t kScrim = 0x00000080;
  // Keyboard-only focus ring (focus-visible): orange accent, never
  // painted for pointer (see Bridge.h paintFocusRing).
  static constexpr std::uint32_t kFocusRing = 0xff6a2bff;
  static constexpr std::uint32_t kLayerFallback = 0x2c2c2eff;
  static constexpr float kGlassThickness = 16.0f;
  static constexpr int kCardRadius = 12;
  static constexpr int kButtonRadius = 7;
  // Gel highlights, translucent white.
  static constexpr std::uint32_t kGlossLine = 0xffffff4d;
  static constexpr std::uint32_t kGlossSheen = 0xffffff14;
  // Knob edge: faint light veil. The slider knob straddles two
  // backgrounds at once (bright fill / dark track), so no dark ring
  // can blend both sides — dark trims fringe against the fill, and
  // read as dirt. A light veil glows on dark and vanishes on bright.
  static constexpr std::uint32_t kThumbRim = 0xffffff66;
  static constexpr std::uint32_t kCardTop = 0x35353bff;
  static constexpr std::uint32_t kCardBottom = 0x262628ff;
  static constexpr std::uint32_t kButtonSecondaryTop = 0x4c4c55ff;
  static constexpr std::uint32_t kButtonSecondaryBottom = 0x333338ff;
  static constexpr std::uint32_t kThumbTop = 0xffffffff;
  static constexpr std::uint32_t kThumbBottom = 0xd4d4daff;
  // Groove the fill travels in: flat dark, one step above the window
  // so the track edge reads on near-black backgrounds.
  static constexpr std::uint32_t kTrackTop = 0x26262cff;
  static constexpr std::uint32_t kTrackBottom = 0x26262cff;
  // Gel faces take a light translucent rim, never a dark one —
  // dark rings on a dark theme close the UI in instead of airing it.
  // Kept quiet (20% dome, 25% edge): stronger washes saturated
  // bases toward gray and the 1px line reads as a white wire.
  static constexpr std::uint32_t kGelGloss = 0xffffff33;
  static constexpr std::uint32_t kGelEdge = 0xffffff40;
  // Soft drop shadow for thumbs and knobs (1-2px offset fill under).
  static constexpr std::uint32_t kShadow = 0x00000066;
  static constexpr std::uint32_t kAccentTop = 0xff8a4dff;
  static constexpr std::uint32_t kAccentBottom = 0xe04a00ff;
};

}  // namespace toolkit
}  // namespace glim
