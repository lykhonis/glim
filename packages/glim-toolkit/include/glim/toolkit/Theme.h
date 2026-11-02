#pragma once

#include <cstdint>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {

// Default look. Values mirror Tokens; elements read these through the
// engine instead of naming Tokens at paint time, so an app re-skins by
// passing a themed engine (see withTheme) with zero element edits.
struct Theme {
  std::uint32_t windowBg = Tokens::kWindowBg;
  std::uint32_t cardFill = Tokens::kCardFill;
  std::uint32_t cardStroke = Tokens::kCardStroke;
  std::uint32_t buttonFill = Tokens::kButtonFill;
  std::uint32_t buttonPrimaryTop = Tokens::kButtonPrimaryTop;
  std::uint32_t buttonPrimaryBottom = Tokens::kButtonPrimaryBottom;
  std::uint32_t accent = Tokens::kAccent;
  std::uint32_t inkOnLight = Tokens::kInkOnLight;
  std::uint32_t inkSecondary = Tokens::kInkSecondary;
  std::uint32_t hairline = Tokens::kHairline;
  std::uint32_t focusRing = Tokens::kFocusRing;
  std::uint32_t layerFallback = Tokens::kLayerFallback;
  float typeLine = Tokens::kTypeLine;
  float typeDisplay = Tokens::kTypeDisplay;
  float glassThickness = Tokens::kGlassThickness;
  int buttonRadius = Tokens::kButtonRadius;
  std::uint32_t glossLine = Tokens::kGlossLine;
  std::uint32_t thumbRim = Tokens::kThumbRim;
  std::uint32_t glossSheen = Tokens::kGlossSheen;
  std::uint32_t cardTop = Tokens::kCardTop;
  std::uint32_t cardBottom = Tokens::kCardBottom;
  std::uint32_t buttonSecondaryTop = Tokens::kButtonSecondaryTop;
  std::uint32_t buttonSecondaryBottom = Tokens::kButtonSecondaryBottom;
  std::uint32_t thumbTop = Tokens::kThumbTop;
  std::uint32_t thumbBottom = Tokens::kThumbBottom;
  std::uint32_t trackTop = Tokens::kTrackTop;
  std::uint32_t trackBottom = Tokens::kTrackBottom;
  std::uint32_t accentTop = Tokens::kAccentTop;
  std::uint32_t accentBottom = Tokens::kAccentBottom;
  // Aqua gel coat + bright inner top edge for gel faces.
  std::uint32_t gelGloss = Tokens::kGelGloss;
  std::uint32_t gelEdge = Tokens::kGelEdge;
};

inline const Theme& theme(const Engine& e) {
  if (const Theme* t = e.get<Theme>()) return *t;
  static const Theme def{};
  return def;
}

// Borrowed theme for one walk; the caller retains t and slot.
inline Engine withTheme(const Engine& e, const Theme& t, Env& slot) {
  slot = Env{&t, &typeid(Theme), e.env};
  return Engine{&slot};
}

}  // namespace toolkit
}  // namespace glim
