#pragma once

#include <string>
#include <utility>

#include "Gel.h"
#include "ShowcaseTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Checkbox : WithSemantic {
  static constexpr const char* kKind = "Check";
  static constexpr const char* kKindOn = "Check.on";
  static constexpr float kBox = 24.0f;
  std::string label;
  State<bool> value = {"value", false};
  Anim<float> pos = {"pos", *value ? 1.0f : 0.0f};
  Box place(Constraints c, const Engine& e) const {
    Box t = text::measureText(label, {0, kUnbounded, 0, kUnbounded}, e);
    return detail::resolveSize(t.w + kBox + 8.0f, kBox, c);
  }
  void outer(const Box& box, Box& out, Box& inner) const {
    float by = box.y + (box.h - kBox) * 0.5f;
    out = {box.x, by, kBox, kBox};
    inner = {box.x + 3.0f, by + 3.0f, kBox - 6.0f, kBox - 6.0f};
  }
  Box labelBox(const Box& box) const {
    float by = box.y + (box.h - kBox) * 0.5f;
    return {
        box.x + kBox + 8.0f,
        by + (kBox - Tokens::kTypeLine) * 0.5f,
        box.w - kBox - 8.0f,
        Tokens::kTypeLine
    };
  }
  void record(const Box& box, Rec& rec) const {
    Box out, inner;
    outer(box, out, inner);
    rec.push(*value ? kKindOn : kKind, out, {}, {}, 6);
    Box lb = labelBox(box);
    rec.push("Lines", lb, label);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    Box out, inner;
    outer(box, out, inner);
    if (*pos < 1.0f) {
      sink.fillGradient(
          out,
          6,
          themeColor(
              e, kShowButtonSecondaryTop, Show::kButtonSecondaryTop
          ),
          themeColor(
              e,
              kShowButtonSecondaryBottom,
              Show::kButtonSecondaryBottom
          )
      );
    }
    gelCoat(sink, out, 6);
    sink.stroke(
        out, 6, 1.0f, themeColor(e, kShowCardStroke, Show::kCardStroke)
    );
    if (*pos > 0.0f) {
      const float a = *pos;
      sink.fillGradient(
          out,
          6,
          withAlpha(themeColor(e, kShowAccentTop, Show::kAccentTop), a),
          withAlpha(
              themeColor(e, kShowAccentBottom, Show::kAccentBottom), a
          )
      );
      // No glyph run: the Latin atlas has no checkmark.
      float s = 10.0f;
      Box mark{
          out.x + (out.w - s) * 0.5f, out.y + (out.h - s) * 0.5f, s, s
      };
      sink.fillRounded(
          mark,
          3,
          withAlpha(
              themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight), a
          )
      );
    }
    Box lb = labelBox(box);
    sink.text(
        lb.x,
        lb.y,
        label,
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight),
        themeFloat(e, kThemeTypeLine, Tokens::kTypeLine)
    );
  }
  bool handle(const Event& ev) const {
    if (ev.is<PointerDown>() || ev.is<Activate>()) {
      value = !*value;
      pos = *value ? 1.0f : 0.0f;
      return true;
    }
    return false;
  }
};

template <class... As>
Block checkbox(Checkbox c, As&&... as) {
  configureTags(c, std::forward<As>(as)...);
  return finish(std::move(c), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
