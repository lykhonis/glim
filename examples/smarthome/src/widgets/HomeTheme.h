#pragma once

// SmartHome theme (user space): app vocabulary via themeKey(), plain
// constants as fallback. Two designed ramps (dark default, light);
// switching is a scope change — widgets only read keys.
#include <cstdint>

#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

inline constexpr ThemeKey kShowCardTop{themeKey("smarthome.cardTop")};
inline constexpr ThemeKey kShowCardBottom{
    themeKey("smarthome.cardBottom")
};
inline constexpr ThemeKey kShowCardStroke{
    themeKey("smarthome.cardStroke")
};
inline constexpr ThemeKey kShowAccentTop{themeKey("smarthome.accentTop")
};
inline constexpr ThemeKey kShowAccentBottom{
    themeKey("smarthome.accentBottom")
};
inline constexpr ThemeKey kShowTrackTop{themeKey("smarthome.trackTop")};
inline constexpr ThemeKey kShowThumbTop{themeKey("smarthome.thumbTop")};
inline constexpr ThemeKey kShowThumbRim{themeKey("smarthome.thumbRim")};
// Type hierarchy: title (~20) and KPI value (~28); body stays on
// kThemeTypeLine (14).
inline constexpr ThemeKey kShowTitleSize{themeKey("smarthome.titleSize")
};
inline constexpr ThemeKey kShowValueSize{themeKey("smarthome.valueSize")
};
inline constexpr ThemeKey kShowHeroSize{themeKey("smarthome.heroSize")};
// Window backdrop: the wash under the villa photo, plus the veil that
// keeps type readable and carries the dark theme's night cast.
inline constexpr ThemeKey kShowBgTop{themeKey("smarthome.bgTop")};
inline constexpr ThemeKey kShowBgBottom{themeKey("smarthome.bgBottom")};
inline constexpr ThemeKey kShowGlowWarm{themeKey("smarthome.glowWarm")};
inline constexpr ThemeKey kShowGlowCool{themeKey("smarthome.glowCool")};
inline constexpr ThemeKey kShowScrimTop{themeKey("smarthome.scrimTop")};
inline constexpr ThemeKey kShowScrimBottom{
    themeKey("smarthome.scrimBottom")
};
// Status dots: ok / warn / bad.
inline constexpr ThemeKey kShowOk{themeKey("smarthome.ok")};

struct Show {
  static constexpr std::uint32_t kCardTop = 0x111114ff;
  static constexpr std::uint32_t kCardBottom = 0x0e0e11ff;
  static constexpr std::uint32_t kCardStroke = 0xffffff47;
  static constexpr int kPadCard = 16;
  static constexpr std::uint32_t kAccentTop = 0xff8a4dff;
  static constexpr std::uint32_t kAccentBottom = 0xe04a00ff;
  static constexpr std::uint32_t kTrackTop = 0x2c3444ff;
  static constexpr std::uint32_t kThumbTop = 0xffffffff;
  static constexpr std::uint32_t kThumbRim = 0xffffff66;
  static constexpr std::uint32_t kGelGloss = 0xffffff33;
  static constexpr std::uint32_t kGelEdge = 0xffffff40;
  static constexpr std::uint32_t kShadow = 0x00000066;
  static constexpr float kTitleSize = 20.0f;
  static constexpr float kValueSize = 28.0f;
  static constexpr float kHeroSize = 56.0f;
  static constexpr std::uint32_t kBgTop = 0x10151cff;
  static constexpr std::uint32_t kBgBottom = 0x07090eff;
  static constexpr std::uint32_t kGlowWarm = 0xff7a3d47u;
  static constexpr std::uint32_t kGlowCool = 0x3d8bff40u;
  // Cool night milk: dark enough for light type, thin enough that the
  // villa still reads through the frost.
  static constexpr std::uint32_t kGlassTint = 0x0c101860u;
  static constexpr std::uint32_t kScrimTop = 0x07090c55u;
  static constexpr std::uint32_t kScrimBottom = 0x07090c44u;
  static constexpr std::uint32_t kOk = 0x34c759ff;
};

// Light ramp: real second palette, not an inversion. Same accent,
// light surfaces, dark ink.
struct ShowLight {
  static constexpr std::uint32_t kWindowBg = 0xeceef1ff;
  static constexpr std::uint32_t kInk = 0x1a1d24ff;
  static constexpr std::uint32_t kCardTop = 0xffffffff;
  static constexpr std::uint32_t kCardBottom = 0xf1f3f6ff;
  static constexpr std::uint32_t kCardStroke = 0xffffffb3;
  static constexpr std::uint32_t kTrackTop = 0xe0e3eaff;
  static constexpr std::uint32_t kThumbTop = 0xffffffff;
  static constexpr std::uint32_t kThumbRim = 0x00000029;
  static constexpr std::uint32_t kBgTop = 0xe9eef5ff;
  static constexpr std::uint32_t kBgBottom = 0xd9dfe8ff;
  static constexpr std::uint32_t kGlowWarm = 0xff9a5c59u;
  static constexpr std::uint32_t kGlowCool = 0x7aa7ff59u;
  // Warm white milk, light enough to stay slightly frosted.
  static constexpr std::uint32_t kGlassTint = 0xfff8f020u;
  // Daylight carries the type on its own: no veil over the photo.
  static constexpr std::uint32_t kScrimTop = 0x00000000u;
  static constexpr std::uint32_t kScrimBottom = 0x00000000u;
};

struct ShowDark {
  static constexpr std::uint32_t kWindowBg = 0x07090eff;
  static constexpr std::uint32_t kInk = 0xe8eaf0ff;
};

// Root scope for the dashboard: one themed() holding the selected
// ramp. Widgets keep their Show::* fallbacks, so dark == current
// pixels and light only changes what the ramp overrides.
struct SmartHomeTheme {
  bool light = false;
  Block child;
};

inline Block smartHomeThemed(SmartHomeTheme t) {
  const bool light = t.light;
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
      themeColor(kShowAccentTop, Show::kAccentTop),
      themeColor(kShowAccentBottom, Show::kAccentBottom),
      themeColor(
          kShowTrackTop, light ? ShowLight::kTrackTop : Show::kTrackTop
      ),
      themeColor(
          kShowThumbTop, light ? ShowLight::kThumbTop : Show::kThumbTop
      ),
      themeColor(
          kShowThumbRim, light ? ShowLight::kThumbRim : Show::kThumbRim
      ),
      themeFloat(kShowTitleSize, Show::kTitleSize),
      themeFloat(kShowValueSize, Show::kValueSize),
      themeFloat(kShowHeroSize, Show::kHeroSize),
      themeColor(kShowBgTop, light ? ShowLight::kBgTop : Show::kBgTop),
      themeColor(
          kShowBgBottom, light ? ShowLight::kBgBottom : Show::kBgBottom
      ),
      themeColor(
          kShowGlowWarm, light ? ShowLight::kGlowWarm : Show::kGlowWarm
      ),
      themeColor(
          kShowGlowCool, light ? ShowLight::kGlowCool : Show::kGlowCool
      ),
      themeColor(
          kShowScrimTop, light ? ShowLight::kScrimTop : Show::kScrimTop
      ),
      themeColor(
          kShowScrimBottom,
          light ? ShowLight::kScrimBottom : Show::kScrimBottom
      ),
      themeColor(kShowOk, Show::kOk),
      std::move(t.child)
  );
}

}  // namespace toolkit
}  // namespace glim
