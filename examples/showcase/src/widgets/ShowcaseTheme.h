#pragma once

// Showcase theme (user space): app vocabulary via themeKey(), plain
// constants as fallback. Two designed ramps (dark default, light);
// switching is a scope change — widgets only read keys.
#include <cstdint>

#include "glim/toolkit.h"

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
// Type hierarchy: title (~20) and KPI value (~28); body stays on
// kThemeTypeLine (14).
inline constexpr ThemeKey kShowTitleSize{themeKey("showcase.titleSize")
};
inline constexpr ThemeKey kShowValueSize{themeKey("showcase.valueSize")
};
// Status dots: ok / warn / bad.
inline constexpr ThemeKey kShowOk{themeKey("showcase.ok")};
inline constexpr ThemeKey kShowWarn{themeKey("showcase.warn")};
inline constexpr ThemeKey kShowBad{themeKey("showcase.bad")};

struct Show {
  static constexpr std::uint32_t kCardTop = 0x111114ff;
  static constexpr std::uint32_t kCardBottom = 0x0e0e11ff;
  static constexpr std::uint32_t kCardStroke = 0xffffff24;
  static constexpr std::uint32_t kButtonPrimaryTop = 0xff7a3dff;
  static constexpr std::uint32_t kButtonPrimaryBottom = 0xf43f00ff;
  static constexpr std::uint32_t kButtonSecondaryTop = 0x4c4c55ff;
  static constexpr std::uint32_t kButtonSecondaryBottom = 0x333338ff;
  static constexpr int kButtonRadius = 7;
  static constexpr int kButtonPadH = 10;
  static constexpr int kButtonPadV = 7;
  static constexpr int kPadCard = 16;
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
  static constexpr float kTitleSize = 20.0f;
  static constexpr float kValueSize = 28.0f;
  static constexpr std::uint32_t kOk = 0x34c759ff;
  static constexpr std::uint32_t kWarn = 0xffcc00ff;
  static constexpr std::uint32_t kBad = 0xff3b30ff;
};

// Light ramp: real second palette, not an inversion. Same accent,
// light surfaces, dark ink.
struct ShowLight {
  static constexpr std::uint32_t kWindowBg = 0xeceef1ff;
  static constexpr std::uint32_t kInk = 0x1a1d24ff;
  static constexpr std::uint32_t kCardTop = 0xffffffff;
  static constexpr std::uint32_t kCardBottom = 0xf1f3f6ff;
  static constexpr std::uint32_t kCardStroke = 0x0000001f;
  static constexpr std::uint32_t kButtonSecondaryTop = 0xffffffff;
  static constexpr std::uint32_t kButtonSecondaryBottom = 0xe2e5ecff;
  static constexpr std::uint32_t kTrackTop = 0xe0e3eaff;
  static constexpr std::uint32_t kTrackBottom = 0xe0e3eaff;
  static constexpr std::uint32_t kThumbTop = 0xffffffff;
  static constexpr std::uint32_t kThumbBottom = 0xdfe2e9ff;
  static constexpr std::uint32_t kThumbRim = 0x00000029;
  static constexpr std::uint32_t kGlossSheen = 0xffffff66;
};

struct ShowDark {
  static constexpr std::uint32_t kWindowBg = 0x0a0a0cff;
  static constexpr std::uint32_t kInk = 0xe8eaf0ff;
};

// Root scope for the dashboard: one themed() holding the selected
// ramp. Widgets keep their Show::* fallbacks, so dark == current
// pixels and light only changes what the ramp overrides.
inline Block showcaseThemed(int theme, Block child) {
  const bool light = theme == 1;
  return themed(
      themeColor(
          kThemeWindowBg,
          light ? ShowLight::kWindowBg : ShowDark::kWindowBg
      ),
      themeColor(
          kThemeInkOnLight, light ? ShowLight::kInk : ShowDark::kInk
      ),
      themeColor(
          kShowCardTop, light ? ShowLight::kCardTop : Show::kCardTop
      ),
      themeColor(
          kShowCardBottom,
          light ? ShowLight::kCardBottom : Show::kCardBottom
      ),
      themeColor(
          kShowCardStroke,
          light ? ShowLight::kCardStroke : Show::kCardStroke
      ),
      themeColor(kShowButtonPrimaryTop, Show::kButtonPrimaryTop),
      themeColor(kShowButtonPrimaryBottom, Show::kButtonPrimaryBottom),
      themeColor(
          kShowButtonSecondaryTop,
          light ? ShowLight::kButtonSecondaryTop
                : Show::kButtonSecondaryTop
      ),
      themeColor(
          kShowButtonSecondaryBottom,
          light ? ShowLight::kButtonSecondaryBottom
                : Show::kButtonSecondaryBottom
      ),
      themeColor(kShowAccentTop, Show::kAccentTop),
      themeColor(kShowAccentBottom, Show::kAccentBottom),
      themeColor(
          kShowTrackTop, light ? ShowLight::kTrackTop : Show::kTrackTop
      ),
      themeColor(
          kShowTrackBottom,
          light ? ShowLight::kTrackBottom : Show::kTrackBottom
      ),
      themeColor(
          kShowThumbTop, light ? ShowLight::kThumbTop : Show::kThumbTop
      ),
      themeColor(
          kShowThumbBottom,
          light ? ShowLight::kThumbBottom : Show::kThumbBottom
      ),
      themeColor(
          kShowThumbRim, light ? ShowLight::kThumbRim : Show::kThumbRim
      ),
      themeColor(
          kShowGlossSheen,
          light ? ShowLight::kGlossSheen : Show::kGlossSheen
      ),
      themeFloat(kShowTitleSize, Show::kTitleSize),
      themeFloat(kShowValueSize, Show::kValueSize),
      themeColor(kShowOk, Show::kOk),
      themeColor(kShowWarn, Show::kWarn),
      themeColor(kShowBad, Show::kBad),
      std::move(child)
  );
}

}  // namespace toolkit
}  // namespace glim
