#pragma once

// Sidebar nav item: tap row with a selected pill. Focusable, so
// keyboard users can switch pages; paint is a flat fill + label, no
// glass cost. Collapsed mode renders a centered glyph instead of the
// label.

#include <string>
#include <utility>

#include "Gel.h"
#include "ShowcaseTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct NavItem {
  static constexpr const char* kKind = "Nav";
  static constexpr const char* kKindActive = "Nav.active";
  std::string label;
  std::string glyph;
  bool selected = false;
  // Animated selection mirror, driven by the tree. Semantics stay
  // boolean.
  Anim<float> sel = {"sel", 0.0f};
  bool collapsed = false;
  Action onTap;
  Box place(Constraints c, const Engine& e) const {
    if (collapsed) return detail::resolveSize(40.0f, 40.0f, c);
    // Full-bleed rows in the rail: the pill spans the column width
    // so all rows share left/right edges; labels stay left-docked.
    if (!isUnbounded(c.maxW))
      return detail::resolveSize(c.maxW, 36.0f, c);
    Box t = text::measureText(label, {0, kUnbounded, 0, kUnbounded}, e);
    return detail::resolveSize(t.w + 28.0f, 36.0f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(
        selected ? kKindActive : kKind, box, collapsed ? glyph : label
    );
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const std::uint32_t ink =
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight);
    // Cross-fade: base wash deepens with selection; the accent bar
    // fades in on top. Steady states match the flat values.
    const float t = *sel;
    sink.fillRounded(box, 8, withAlpha(ink, 0.05f + 0.07f * t));
    if (t > 0.003f) {
      // HUD index bar: single accent tick at the leading edge.
      Box bar{box.x + 6.0f, box.y + 8.0f, 3.0f, box.h - 16.0f};
      if (bar.h < 8.0f) bar.h = 8.0f;
      sink.fillRounded(
          bar,
          1,
          withAlpha(themeColor(e, kShowAccentTop, Show::kAccentTop), t)
      );
    }
    const float size = themeFloat(e, kThemeTypeLine, Tokens::kTypeLine);
    if (collapsed) {
      float gw =
          static_cast<float>(glyph.size()) * Tokens::kTypeAdvance;
      sink.text(
          box.x + (box.w - gw) * 0.5f,
          box.y + (box.h - size) * 0.5f,
          glyph,
          ink,
          size
      );
    } else {
      sink.text(
          box.x + 14.0f, box.y + (box.h - size) * 0.5f, label, ink, size
      );
    }
  }
  bool handle(const Event& ev) const {
    if (ev.is<PointerDown>() || ev.is<Activate>())
      return detail::invokeAction(onTap);
    return false;
  }
};

template <class... As>
auto navGlyph(NavItem n, As&&... as) {
  configureTags(n, std::forward<As>(as)...);
  return finish(std::move(n), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
