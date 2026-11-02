#pragma once

#include <string>
#include <utility>

#include "glim/toolkit/Action.h"
#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Theme.h"
#include "glim/toolkit/Tokens.h"

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
    Box t =
        detail::measureText(label, {0, kUnbounded, 0, kUnbounded}, e);
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
    const Theme& t = theme(e);
    if (isPrimary) {
      sink.fillGradient(
          box,
          t.buttonRadius,
          t.buttonPrimaryTop,
          t.buttonPrimaryBottom,
          semantic
      );
      if (box.h > 10.0f && box.w > 8.0f) {
        Box sheen{
            detail::snapHalf(box.x + 2.0f),
            detail::snapHalf(box.y + 2.0f),
            box.w - 4.0f,
            box.h * 0.42f
        };
        sink.fillRounded(
            sheen,
            t.buttonRadius > 2 ? t.buttonRadius - 2 : 1,
            t.glossSheen,
            semantic
        );
        Box line{
            detail::snapHalf(box.x + 3.0f),
            detail::snapHalf(box.y + 1.0f),
            box.w - 6.0f,
            1.0f
        };
        sink.fillRounded(line, 1, t.glossLine, semantic);
      }
      sink.stroke(box, t.buttonRadius, 1.0f, t.cardStroke, semantic);
    } else {
      sink.fillGradient(
          box,
          t.buttonRadius,
          t.buttonSecondaryTop,
          t.buttonSecondaryBottom,
          semantic
      );
      if (box.w > 8.0f) {
        Box line{
            detail::snapHalf(box.x + 3.0f),
            detail::snapHalf(box.y + 1.0f),
            box.w - 6.0f,
            1.0f
        };
        sink.fillRounded(line, 1, t.glossLine, semantic);
      }
      sink.stroke(box, t.buttonRadius, 1.0f, t.cardStroke, semantic);
    }
    sink.text(
        box.x + static_cast<float>(padH),
        box.y + (box.h - t.typeLine) * 0.5f,
        label,
        t.inkOnLight,
        t.typeLine,
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
