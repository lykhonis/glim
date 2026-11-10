#pragma once

// Gauge: a half dial of radial ticks lit up to the value, with a knob
// at the tip.
//
// The dial is its own control: press or drag anywhere over it and the
// value follows the angle around the dial's center; Left/Right step.
// `value` is the level cell (0..1); `shown` is the level to draw, so
// the caller can ease it. Off, the lit ticks dim.

#include <algorithm>
#include <cmath>
#include <utility>

#include "Gel.h"
#include "HomeTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Gauge : WithSemantic {
  static constexpr const char* kKind = "Gauge";
  static constexpr float kH = 112.0f;
  static constexpr int kTicks = 25;
  static constexpr float kStep = 0.05f;  // key step
  static constexpr float kGrain =
      0.01f;  // drag writes land on whole percents
  State<float> value = {"value", 0.0f};
  float shown = 0.0f;
  bool on = true;

  struct Dial {
    float cx, cy, r;
  };
  static Dial dial(const Box& box) {
    return {
        box.x + box.w * 0.5f,
        box.y + box.h - 2.0f,
        std::min(box.w * 0.5f - 12.0f, box.h - 14.0f)
    };
  }
  // Level under a point: the angle swept from the dial's left end,
  // pinned to the ends below the center line.
  static float levelAt(const Box& box, float x, float y) {
    const Dial d = dial(box);
    if (y >= d.cy) return x < d.cx ? 0.0f : 1.0f;
    const float deg = std::atan2(y - d.cy, x - d.cx) * 180.0f /
                      3.14159265f;  // -180..0
    return std::clamp(
        std::round((deg + 180.0f) / 180.0f / kGrain) * kGrain,
        0.0f,
        1.0f
    );
  }

  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(
        isUnbounded(c.maxW) ? 240.0f : c.maxW, kH, c
    );
  }
  void record(const Box& box, Rec& rec) const { rec.push(kKind, box); }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const std::uint32_t ink =
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight);
    const std::uint32_t lit = withAlpha(
        themeColor(e, kShowAccentTop, Show::kAccentTop),
        on ? 1.0f : 0.35f
    );
    const Dial d = dial(box);
    const float level = std::clamp(shown, 0.0f, 1.0f);
    constexpr float kDeg = 3.14159265f / 180.0f;
    for (int i = 0; i < kTicks; ++i) {
      const float t =
          static_cast<float>(i) / static_cast<float>(kTicks - 1);
      const float a = (180.0f + 180.0f * t) * kDeg;
      const float xy[4] = {
          d.cx + (d.r - 22.0f) * std::cos(a),
          d.cy + (d.r - 22.0f) * std::sin(a),
          d.cx + d.r * std::cos(a),
          d.cy + d.r * std::sin(a),
      };
      sink.polyline(
          xy, 2, 3.0f, 0.0f, t <= level ? lit : withAlpha(ink, 0.18f)
      );
    }
    const float a = (180.0f + 180.0f * level) * kDeg;
    const float kx = d.cx + (d.r + 8.0f) * std::cos(a);
    const float ky = d.cy + (d.r + 8.0f) * std::sin(a);
    sink.arc(kx, ky, 6.0f, 6.0f, 0.0f, 360.0f, withAlpha(lit, 0.3f));
    sink.arc(kx, ky, 2.5f, 5.0f, 0.0f, 360.0f, lit);
  }

  bool handleAt(const Box& box, const Event& ev) const {
    float x = 0.0f, y = 0.0f;
    if (auto p = ev.as<PointerDown>()) {
      x = p->get().x, y = p->get().y;
    } else if (auto m = ev.as<PointerMove>()) {
      if (!m->get().pressed) return false;
      x = m->get().x, y = m->get().y;
    } else {
      return ev.is<PointerUp>();
    }
    const float next = levelAt(box, x, y);
    if (next == *value) return ev.is<PointerDown>();
    value = next;
    return true;
  }
  bool handle(const Event& ev) const {
    if (auto k = ev.as<KeyPress>()) {
      const Key key = k->get().key;
      if (key != Key::Left && key != Key::Right) return false;
      value = std::clamp(
          *value + (key == Key::Right ? kStep : -kStep), 0.0f, 1.0f
      );
      return true;
    }
    return ev.is<Activate>();
  }
};

template <class... As>
Block gauge(Gauge g, As&&... as) {
  // Tags land on the fields only: a wrapper would hide the gesture
  // hooks.
  configureTags(g, std::forward<As>(as)...);
  return finish(std::move(g));
}

}  // namespace toolkit
}  // namespace glim
