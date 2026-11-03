#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "Gel.h"
#include "glim/toolkit/Action.h"
#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Tokens.h"
#include "glim/toolkit/text/Text.h"
#include "glim/toolkit/theme/Keys.h"

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
    rec.push(isPrimary ? kKindPrimary : kKind, box, label, semantic);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const int radius =
        themeInt(e, kThemeButtonRadius, Tokens::kButtonRadius);
    const std::uint32_t stroke =
        themeColor(e, kThemeCardStroke, Tokens::kCardStroke);
    const float line = themeFloat(e, kThemeTypeLine, Tokens::kTypeLine);
    if (isPrimary) {
      sink.fillGradient(
          box,
          radius,
          themeColor(
              e, kThemeButtonPrimaryTop, Tokens::kButtonPrimaryTop
          ),
          themeColor(
              e, kThemeButtonPrimaryBottom, Tokens::kButtonPrimaryBottom
          ),
          semantic
      );
      gelCoat(sink, box, radius, semantic);
      sink.stroke(box, radius, 1.0f, stroke, semantic);
    } else {
      sink.fillGradient(
          box,
          radius,
          themeColor(
              e, kThemeButtonSecondaryTop, Tokens::kButtonSecondaryTop
          ),
          themeColor(
              e,
              kThemeButtonSecondaryBottom,
              Tokens::kButtonSecondaryBottom
          ),
          semantic
      );
      gelCoat(sink, box, radius, semantic);
      sink.stroke(box, radius, 1.0f, stroke, semantic);
    }
    sink.text(
        box.x + static_cast<float>(padH),
        box.y + (box.h - line) * 0.5f,
        label,
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight),
        line,
        semantic
    );
  }
  bool handle(const Event& ev) const {
    if (ev.is<PointerDown>() || ev.is<Activate>())
      return detail::invokeAction(onTap);
    return false;
  }
};

template <class... As>
Block button(std::string label, Action onTap, As&&... as) {
  Button b;
  b.label = std::move(label);
  b.onTap = std::move(onTap);
  b.padH = Tokens::kButtonPadH;
  b.padV = Tokens::kButtonPadV;
  configureTags(b, std::forward<As>(as)...);
  return finish(std::move(b), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
