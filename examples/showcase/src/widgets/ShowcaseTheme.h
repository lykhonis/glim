#pragma once

// Showcase theme (user space). Toolkit owns only four keys — window
// background, ink, type size, focus ring (see toolkit/theme/Keys.h).
// Everything below is app vocabulary: the frosted-card/button/gel
// ramp, defined here by the app via themeKey(), with plain constants
// as fallback. Same values as the old toolkit Tokens, so pixels do
// not change; only the owner does.
#include <cstdint>

#include "glim/toolkit/theme/Keys.h"

namespace glim {
namespace toolkit {

inline constexpr ThemeKey kShowCardTop{themeKey("showcase.cardTop")};
inline constexpr ThemeKey kShowCardBottom{themeKey("showcase.cardBottom"
)};
inline constexpr ThemeKey kShowCardStroke{themeKey("showcase.cardStroke"
)};
inline constexpr ThemeKey kShowButtonPrimaryTop{
    themeKey("showcase.buttonPrimaryTop")
};
inline constexpr ThemeKey kShowButtonPrimaryBottom{
    themeKey("showcase.buttonPrimaryBottom")
};
inline constexpr ThemeKey kShowButtonSecondaryTop{
    themeKey("showcase.buttonSecondaryTop")
};
inline constexpr ThemeKey kShowButtonSecondaryBottom{
    themeKey("showcase.buttonSecondaryBottom")
};
inline constexpr ThemeKey kShowButtonRadius{
    themeKey("showcase.buttonRadius")
};
inline constexpr ThemeKey kShowAccentTop{themeKey("showcase.accentTop")
};
inline constexpr ThemeKey kShowAccentBottom{
    themeKey("showcase.accentBottom")
};
inline constexpr ThemeKey kShowTrackTop{themeKey("showcase.trackTop")};
inline constexpr ThemeKey kShowTrackBottom{
    themeKey("showcase.trackBottom")
};
inline constexpr ThemeKey kShowThumbTop{themeKey("showcase.thumbTop")};
inline constexpr ThemeKey kShowThumbBottom{
    themeKey("showcase.thumbBottom")
};
inline constexpr ThemeKey kShowThumbRim{themeKey("showcase.thumbRim")};
inline constexpr ThemeKey kShowGlossSheen{themeKey("showcase.glossSheen"
)};

struct Show {
  static constexpr std::uint32_t kCardTop = 0x35353bff;
  static constexpr std::uint32_t kCardBottom = 0x262628ff;
  static constexpr std::uint32_t kCardStroke = 0xffffff29;
  static constexpr std::uint32_t kButtonPrimaryTop = 0xff7a3dff;
  static constexpr std::uint32_t kButtonPrimaryBottom = 0xf43f00ff;
  static constexpr std::uint32_t kButtonSecondaryTop = 0x4c4c55ff;
  static constexpr std::uint32_t kButtonSecondaryBottom = 0x333338ff;
  static constexpr int kButtonRadius = 7;
  static constexpr int kButtonPadH = 10;
  static constexpr int kButtonPadV = 7;
  static constexpr int kPadCard = 12;
  static constexpr std::uint32_t kAccentTop = 0xff8a4dff;
  static constexpr std::uint32_t kAccentBottom = 0xe04a00ff;
  static constexpr std::uint32_t kTrackTop = 0x26262cff;
  static constexpr std::uint32_t kTrackBottom = 0x26262cff;
  static constexpr std::uint32_t kThumbTop = 0xffffffff;
  static constexpr std::uint32_t kThumbBottom = 0xd4d4daff;
  static constexpr std::uint32_t kThumbRim = 0xffffff66;
  static constexpr std::uint32_t kGlossSheen = 0xffffff14;
  static constexpr std::uint32_t kGelGloss = 0xffffff33;
  static constexpr std::uint32_t kGelEdge = 0xffffff40;
  static constexpr std::uint32_t kShadow = 0x00000066;
};

}  // namespace toolkit
}  // namespace glim
