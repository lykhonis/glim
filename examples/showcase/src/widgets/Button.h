#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "Gel.h"
#include "ShowcaseTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Button : WithGap,
                WithPrimary,
                WithThrough,
                WithPad,
                WithSemantic {
  static constexpr const char* kKind = "Button";
  static constexpr const char* kKindPrimary = "Button.primary";
  std::string label;
  Action onTap;
  Box place(Constraints c, const Engine& e) const {
    Box t = text::measureText(label, {0, kUnbounded, 0, kUnbounded}, e);
    return detail::resolveSize(
        t.w + static_cast<float>(2 * padH),
        t.h + static_cast<float>(2 * padV),
        c
    );
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(isPrimary ? kKindPrimary : kKind, box, label);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const int radius =
        themeInt(e, kShowButtonRadius, Show::kButtonRadius);
    const std::uint32_t stroke =
        themeColor(e, kShowCardStroke, Show::kCardStroke);
    const float line = themeFloat(e, kThemeTypeLine, Tokens::kTypeLine);
    if (isPrimary) {
      sink.fillGradient(
          box,
          radius,
          themeColor(e, kShowButtonPrimaryTop, Show::kButtonPrimaryTop),
          themeColor(
              e, kShowButtonPrimaryBottom, Show::kButtonPrimaryBottom
          )
      );
      gelCoat(sink, box, radius);
      sink.stroke(box, radius, 1.0f, stroke);
    } else {
      sink.fillGradient(
          box,
          radius,
          themeColor(
              e, kShowButtonSecondaryTop, Show::kButtonSecondaryTop
          ),
          themeColor(
              e,
              kShowButtonSecondaryBottom,
              Show::kButtonSecondaryBottom
          )
      );
      gelCoat(sink, box, radius);
      sink.stroke(box, radius, 1.0f, stroke);
    }
    sink.text(
        box.x + static_cast<float>(padH),
        box.y + (box.h - line) * 0.5f,
        label,
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight),
        line
    );
  }
  bool handle(const Event& ev) const {
    if (ev.is<PointerDown>() || ev.is<Activate>())
      return detail::invokeAction(onTap);
    return false;
  }
};

template <class... As>
Block button(Button b, As&&... as) {
  b.padH = Show::kButtonPadH;
  b.padV = Show::kButtonPadV;
  configureTags(b, std::forward<As>(as)...);
  return finish(std::move(b), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
