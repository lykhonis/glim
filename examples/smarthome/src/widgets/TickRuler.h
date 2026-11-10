#pragma once

// TickRuler: whole-number scale centered on the value, with minor ticks
// between, a pill marking the value, and edge numbers fading out.
//
// The ruler is its own control: drag the scale under the needle, flick
// it to spin on, tap a number to go there, or step with Left/Right.
// `value` is the level cell (0..1) across `lo`..`hi`; `shown` is the
// level to draw, so the caller can ease it (a flick moves `value` far
// at once; a longer ease reads as the spin). Releases and taps land on
// whole `step`s.

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "Gel.h"
#include "HomeTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct TickRuler : WithSemantic {
  static constexpr const char* kKind = "TickRuler";
  static constexpr float kH = 92.0f;
  static constexpr int kSpan = 3;           // whole numbers each side
  static constexpr int kMinor = 5;          // ticks per unit
  static constexpr float kDragSlop = 4.0f;  // px before a press drags
  static constexpr float kFlickTau =
      0.25f;  // s of coast the release speed carries
  static constexpr float kFlickMinPx =
      120.0f;  // px/s below which a release just snaps
  static constexpr float kRestSec =
      0.1f;  // a pause this long before release is no flick
  static constexpr float kSpeedMix =
      0.6f;  // weight of the newest speed sample
  State<float> value = {"value", 0.5f};
  float shown = 0.5f;
  float lo = 0.0f;
  float hi = 1.0f;
  float step = 1.0f;
  State<float> grabX = {"grab-x", 0.0f};
  State<float> grabValue = {"grab-value", 0.0f};
  State<bool> dragging = {"dragging", false};
  // Finger speed: the last sample's place and time, and the smoothed
  // px/s.
  State<float> trackX = {"track-x", 0.0f};
  State<float> trackT = {"track-t", 0.0f};
  State<float> speed = {"speed", 0.0f};
  State<float> moveT = {"move-t", 0.0f};

  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(
        isUnbounded(c.maxW) ? 280.0f : c.maxW, kH, c
    );
  }
  static float unitPx(const Box& box) {
    return box.w / static_cast<float>(2 * kSpan + 1);
  }
  float units(float level) const {
    return lo + std::clamp(level, 0.0f, 1.0f) * (hi - lo);
  }
  float level(float u) const {
    return std::clamp((u - lo) / (hi - lo), 0.0f, 1.0f);
  }
  float snapped(float u) const {
    return level(std::round(u / step) * step);
  }

  // Events within one frame share a time, so a sample is taken when the
  // time moves on.
  void track(float x, float t) const {
    moveT = t;
    const float dt = t - *trackT;
    if (dt <= 0.0f) return;
    speed =
        kSpeedMix * (x - *trackX) / dt + (1.0f - kSpeedMix) * *speed;
    trackX = x;
    trackT = t;
  }
  // Units the release carries the scale on: with the finger's own
  // speed, for a while; none after a pause or a slow release.
  float flick(const Box& box, float now) const {
    if (now - *moveT > kRestSec || std::fabs(*speed) < kFlickMinPx)
      return 0.0f;
    return -*speed / unitPx(box) * kFlickTau;
  }

  void record(const Box& box, Rec& rec) const { rec.push(kKind, box); }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const std::uint32_t ink =
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight);
    const float size = themeFloat(e, kThemeTypeLine, Tokens::kTypeLine);
    const float unit = unitPx(box);
    const float mid = box.x + box.w * 0.5f;
    const float at = units(shown);
    const float center = std::round(at);
    auto xOf = [&](float v) { return mid + (v - at) * unit; };

    const Box pill{mid - 20.0f, box.y, 40.0f, box.h};
    sink.fillGradient(
        pill,
        14,
        withAlpha(
            themeColor(e, kShowAccentTop, Show::kAccentTop), 0.28f
        ),
        withAlpha(
            themeColor(e, kShowAccentBottom, Show::kAccentBottom), 0.10f
        )
    );
    sink.stroke(
        pill,
        14,
        1.0f,
        withAlpha(themeColor(e, kShowAccentTop, Show::kAccentTop), 0.6f)
    );

    sink.pushClip(box);
    for (int n = -kSpan - 1; n <= kSpan + 1; ++n) {
      const float v = center + static_cast<float>(n);
      const float x = xOf(v);
      const float fade =
          std::max(0.0f, 1.0f - std::fabs(x - mid) / (box.w * 0.5f));
      const std::string label = std::to_string(static_cast<int>(v));
      const float lw =
          static_cast<float>(label.size()) * Tokens::kTypeAdvance;
      sink.text(
          x - lw * 0.5f,
          box.y + 12.0f,
          label,
          withAlpha(ink, 0.25f + 0.75f * fade),
          size
      );
      for (int m = 0; m < kMinor; ++m) {
        const float tx = x + unit * static_cast<float>(m) /
                                 static_cast<float>(kMinor);
        const float h = m == 0 ? 30.0f : 18.0f;
        sink.fillRounded(
            {tx - 0.75f, box.y + box.h - 12.0f - h, 1.5f, h},
            1,
            withAlpha(ink, 0.1f + 0.3f * fade)
        );
      }
    }
    sink.pop();
    sink.fillRounded(
        {mid - 1.0f, box.y + 34.0f, 2.0f, box.h - 46.0f}, 1, ink
    );
  }

  bool handleAt(const Box& box, const Event& ev) const {
    if (auto d = ev.as<PointerDown>()) {
      grabX = d->get().x;
      grabValue = *value;
      dragging = false;
      trackX = d->get().x;
      trackT = d->get().time;
      moveT = d->get().time;
      speed = 0.0f;
      return true;
    }
    if (auto m = ev.as<PointerMove>()) {
      if (!m->get().pressed) return false;
      track(m->get().x, m->get().time);
      const float dx = m->get().x - *grabX;
      if (!*dragging && std::fabs(dx) < kDragSlop) return false;
      if (!*dragging) dragging = true;
      // The scale follows the finger: dragging right brings lower
      // numbers under the needle. Writes land on 1/10 of a step so
      // sub-pixel moves do not repaint.
      const float u = units(*grabValue) - dx / unitPx(box);
      const float next =
          level(std::round(u / step * 10.0f) * step / 10.0f);
      if (next == *value) return false;
      value = next;
      return true;
    }
    if (auto u = ev.as<PointerUp>()) {
      const float target =
          *dragging
              ? snapped(units(*value) + flick(box, u->get().time))
              : snapped(
                    units(*value) +
                    (u->get().x - (box.x + box.w * 0.5f)) / unitPx(box)
                );
      dragging = false;
      if (target != *value) value = target;
      return true;
    }
    return false;
  }
  bool handle(const Event& ev) const {
    if (auto k = ev.as<KeyPress>()) {
      const Key key = k->get().key;
      if (key != Key::Left && key != Key::Right) return false;
      value =
          snapped(units(*value) + (key == Key::Right ? step : -step));
      return true;
    }
    return ev.is<Activate>();
  }
};

template <class... As>
Block tickRuler(TickRuler r, As&&... as) {
  // Tags land on the fields only: a wrapper would hide the gesture
  // hooks.
  configureTags(r, std::forward<As>(as)...);
  return finish(std::move(r));
}

}  // namespace toolkit
}  // namespace glim
