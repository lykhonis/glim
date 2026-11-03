#pragma once

#include <string>
#include <utility>

#include "Gel.h"
#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Tokens.h"
#include "glim/toolkit/text/Text.h"
#include "glim/toolkit/theme/Keys.h"

namespace glim {
namespace toolkit {

// Labeled checkbox: 24px box plus label, tap flips.
struct Checkbox : WithSemantic {
  static constexpr const char* kKind = "Check";
  static constexpr const char* kKindOn = "Check.on";
  static constexpr float kBox = 24.0f;
  State<bool> value;
  std::string label;
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
    rec.push(*value ? kKindOn : kKind, out, {}, semantic, 6);
    Box lb = labelBox(box);
    rec.push("Lines", lb, label, semantic);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    Box out, inner;
    outer(box, out, inner);
    if (*value) {
      sink.fillGradient(
          out,
          6,
          themeColor(e, kThemeAccentTop, Tokens::kAccentTop),
          themeColor(e, kThemeAccentBottom, Tokens::kAccentBottom),
          semantic
      );
      gelCoat(sink, out, 6, semantic);
      sink.stroke(
          out,
          6,
          1.0f,
          themeColor(e, kThemeCardStroke, Tokens::kCardStroke),
          semantic
      );
    } else {
      sink.fillGradient(
          out,
          6,
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
      gelCoat(sink, out, 6, semantic);
      sink.stroke(
          out,
          6,
          1.0f,
          themeColor(e, kThemeCardStroke, Tokens::kCardStroke),
          semantic
      );
    }
    if (*value) {
      // Check block: centered white inset reads checked at small
      // sizes without a glyph run (Latin atlas has no checkmark).
      float s = 10.0f;
      Box mark{
          out.x + (out.w - s) * 0.5f, out.y + (out.h - s) * 0.5f, s, s
      };
      sink.fillRounded(
          mark,
          3,
          themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight),
          semantic
      );
    }
    Box lb = labelBox(box);
    sink.text(
        lb.x,
        lb.y,
        label,
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight),
        themeFloat(e, kThemeTypeLine, Tokens::kTypeLine),
        semantic
    );
  }
  bool handle(const Event& ev) const {
    if (ev.is<PointerDown>() || ev.is<Activate>()) {
      value = !*value;
      return true;
    }
    return false;
  }
};

template <class... As>
Block checkbox(std::string label, State<bool> v, As&&... as) {
  Checkbox c;
  c.label = std::move(label);
  c.value = std::move(v);
  configureTags(c, std::forward<As>(as)...);
  return finish(std::move(c), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
