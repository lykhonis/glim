#pragma once

// Header bar: collapse chevron, page title, live FPS pill, theme
// segmented. Full-width row; callers wrap it in wide().
#include <cstdio>
#include <string>
#include <utility>

#include "Button.h"
#include "Segmented.h"
#include "ShowcaseTheme.h"
#include "Type.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct FpsPill {
  static constexpr const char* kKind = "Fps";
  float fps = 0.f;
  bool live = false;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(92.0f, 28.0f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 14);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const std::uint32_t ink =
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight);
    sink.fillRounded(box, 14, withAlpha(ink, 0.07f));
    std::uint32_t dot = themeColor(e, kShowOk, Show::kOk);
    if (!live)
      dot = themeColor(e, kShowCardStroke, Show::kCardStroke);
    else if (fps < 30.f)
      dot = themeColor(e, kShowBad, Show::kBad);
    else if (fps < 55.f)
      dot = themeColor(e, kShowWarn, Show::kWarn);
    sink.fillRounded(
        {box.x + 10.0f, box.y + 10.0f, 8.0f, 8.0f}, 4, dot
    );
    char label[32];
    if (live)
      std::snprintf(
          label, sizeof(label), "%.0f FPS", static_cast<double>(fps)
      );
    else
      std::snprintf(label, sizeof(label), "-- FPS");
    sink.text(
        box.x + 24.0f,
        box.y + (box.h - Tokens::kTypeLine) * 0.5f,
        label,
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight),
        themeFloat(e, kThemeTypeLine, Tokens::kTypeLine)
    );
  }
};

template <class... As>
auto fpsPill(FpsPill p, As&&... as) {
  configureTags(p, std::forward<As>(as)...);
  return finish(std::move(p), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
