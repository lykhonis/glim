#include "test_helpers.h"

using namespace glim::toolkit;

void test_theme() {
  {
    constexpr static ThemeKey kTestAccent = themeKey("test.accent");
    constexpr std::uint32_t kTestAccentDefault = 0xff5a1fff;
    struct AccentProbe {
      const char* kind() const { return "AccentProbe"; }
      std::uint32_t* out = nullptr;
      Box place(Constraints c, const Engine& e) const {
        *out = themeColor(e, kTestAccent, kTestAccentDefault);
        return detail::resolveSize(10, 10, c);
      }
    };
    {
      std::uint32_t seen = 0;
      Block t = themed(themeColor(kTestAccent, 0x11223344), toBlock(AccentProbe{&seen}));
      place(t, {0, 720, 0, 480});
      assert(seen == 0x11223344);
    }
    {
      std::uint32_t seen = 0;
      place(toBlock(AccentProbe{&seen}), {0, 720, 0, 480});
      assert(seen == kTestAccentDefault);
    }
    {
      std::uint32_t seen = 0;
      Block t = themed(
        themeColor(kTestAccent, 0x11111111),
        themed(themeColor(kTestAccent, 0x22222222), toBlock(AccentProbe{&seen}))
      );
      place(t, {0, 720, 0, 480});
      assert(seen == 0x22222222);
    }
    {
      std::uint32_t sa = 0, sb = 0;
      Block t = row(
        gap(0),
        themed(themeColor(kTestAccent, 0xaaaaaaaa), toBlock(AccentProbe{&sa})),
        themed(themeColor(kTestAccent, 0xbbbbbbbb), toBlock(AccentProbe{&sb}))
      );
      place(t, {0, 720, 0, 480});
      assert(sa == 0xaaaaaaaa && sb == 0xbbbbbbbb);
    }
    {
      OpsRecorder rec;
      struct Feed {
        std::uint32_t id = 0;
        bool foreign = false;
      } feed;
      Block t = themed(inkOnLight(0x123456ff), lines("hi"));
      Box tb = place(t, {0, 720, 0, 480});
      paintTree(t, tb, Engine{}, rec, feed);
      bool foundText = false;
      for (const auto& o : rec.ops) {
        if (o.kind == Op::Kind::Text) {
          foundText = true;
          assert(o.rgba == 0x123456ff);
        }
      }
      assert(foundText);
    }
  }
  {
    Store st;
    Engine e;
    e.store = st;
    assert(themeColor(e, kThemeFocusRing) == Tokens::kFocusRing);
    assert(themeColor(e, kThemeWindowBg) == Tokens::kWindowBg);
    st.setPrefs({A11yPref::HighContrast});
    assert(prefsOf(e).has(A11yPref::HighContrast));
    assert(themeColor(e, kThemeFocusRing) == Tokens::kFocusRingHighContrast);
    assert(themeColor(e, kThemeWindowBg) == Tokens::kWindowBgHighContrast);
    assert(themeColor(e, kThemeInkOnLight) == Tokens::kInkOnLight);
    assert(themeColor(e, kThemeFocusRing, 0x123456ffu) == 0x123456ffu);
    assert(themeColor(Engine{}, kThemeFocusRing) == Tokens::kFocusRing);
  }
}
