#pragma once

// SpeedGauge (paQys.jpg): 0..300 km/h over 250.4deg from 145deg, bezel
// rings + top/bottom lamps, 13 orange ticks every 25 km/h, minor ticks
// every 5, tapered needle with glow, digital readout and target plate.
// Geometry is measured in reference pixels (center 584,330); the tick
// table keeps the reference's slightly irregular placement.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../InfoTheme.h"
#include "Fui.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct SpeedGauge {
  static constexpr const char* kKind = "SpeedGauge";
  float value = 0.f;  // 0..1 -> 0..300 (readout ring position)
  float diameter = 520.f;
  int speedKmh = 178;
  int targetKmh = 180;
  // The reference needle points at 218.4 km/h while the readout says
  // 178.
  float needleKmh = 218.4f;
  // Readout odometer position (Flight::speedRoll); 0 shows speedKmh
  // still.
  float speedRoll = 0.f;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(diameter, diameter, c);
  }

  // Odometer readout with fixed digit cells: the type and cells come
  // from the reference value "178", and every digit is centered in its
  // cell, so values never reflow. Over the last third of each km/h step
  // the changing digits slide up and cross-fade into the next value.
  void paintReadout(AnySink& sink, const Engine& e, const fui::At& at)
      const {
    using namespace fui;
    constexpr Ink kInk{521, 388, 645, 447};
    static const std::string kCells = "178";
    const int shown = speedRoll > 0.f
                          ? static_cast<int>(std::floor(speedRoll))
                          : speedKmh;
    const float frac =
        speedRoll > 0.f ? speedRoll - std::floor(speedRoll) : 0.f;
    const float u = std::clamp((frac - 0.66f) / 0.34f, 0.f, 1.f);
    const float k = u * u * (3.f - 2.f * u);
    const std::string a = std::to_string(shown),
                      b = std::to_string(shown + 1);
    if (a.size() != kCells.size()) {
      fit(sink, e, at, a, kInk, Ref::kOrange, kNumeralTrack, 1.f);
      return;
    }
    const bool rolling = k > 0.f && b.size() == a.size();
    const Fitted f = fitLayout(
        e, at, kCells, kInk, Ref::kOrange, kNumeralTrack, 1.f
    );
    const float travel = f.type.cap * 0.45f;
    auto faded = [](float alpha) {
      const auto av = static_cast<std::uint32_t>(
          std::clamp(alpha, 0.f, 1.f) * 255.f + 0.5f
      );
      return (Ref::kOrange & 0xffffff00u) |
             ((Ref::kOrange & 0xffu) * av / 255u);
    };
    Type t = f.type;
    const Type bare{t.cap, 0.f, t.condense, t.weight};
    auto glyph = [&](char ch,
                     float cellX,
                     float cellW,
                     float dy,
                     std::uint32_t color) {
      const std::string one(1, ch);
      t.color = color;
      fui::text(
          sink,
          e,
          one,
          cellX + (cellW - measure(e, one, bare)) * 0.5f,
          f.capTop + dy,
          t
      );
    };
    float pen = f.x;
    for (std::size_t i = 0; i < a.size(); ++i) {
      const float cell = measure(e, std::string(1, kCells[i]), bare);
      if (!rolling || a[i] == b[i]) {
        glyph(a[i], pen, cell, 0.f, Ref::kOrange);
      } else {
        // Staggered: the outgoing digit is gone before the incoming one
        // shows, so the two never smear into each other mid-roll.
        glyph(
            a[i],
            pen,
            cell,
            -k * travel,
            faded(1.f - std::clamp(k / 0.55f, 0.f, 1.f))
        );
        glyph(
            b[i],
            pen,
            cell,
            (1.f - k) * travel,
            faded(std::clamp((k - 0.45f) / 0.55f, 0.f, 1.f))
        );
      }
      pen += cell + f.type.track;
    }
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, static_cast<int>(box.w / 2));
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    using namespace fui;
    const At at(box, 324.f, 70.f);
    const float cx = at.x(584.f), cy = at.y(330.f);
    constexpr float kStartDeg = 145.f, kSweepDeg = 250.4f;
    // Bezel and dial face.
    // Bezel rings share the tick center (circle fit of probe `ray`
    // peaks), not the hub.
    const float bx = at.x(583.5f), by = at.y(334.f);
    sink.fillRounded(
        {bx - 225.f, by - 225.f, 450.f, 450.f}, 225, 0x050c12f0
    );
    sink.arc(bx, by, 257.f, 2.f, 120.f, 120.f, 0x6a7378ff);
    sink.arc(bx, by, 257.f, 2.f, 300.f, 120.f, 0x6a7378ff);
    sink.arc(bx, by, 257.f, 1.5f, 240.f, 60.f, 0x3d464dff);
    sink.arc(bx, by, 257.f, 1.5f, 60.f, 60.f, 0x3d464dff);
    sink.arc(bx, by, 243.f, 1.5f, 0.f, 360.f, 0x212a31ff);
    sink.arc(bx, by, 226.f, 2.5f, 0.f, 360.f, 0x555e63ff);
    // Upper dial arc only: the reference has no lower ring (probe rows
    // 380+).
    sink.arc(bx, by, 118.f, 1.f, 162.f, 216.f, 0x424c55ff);
    // Lamps at 12 and 6 o'clock.
    // Lamps: thin hot segment (x 571..594 at r242) with a tight halo.
    constexpr auto kGlow = static_cast<std::uint32_t>(EffectKind::Glow);
    for (const float y : {cy - 247.f, cy + 237.f}) {
      sink.pushEffect(
          {cx - 18.f, y, 36.f, 10.f}, 5, kGlow, Ref::kOrange, 0.3f
      );
      sink.pop();
    }
    sink.arc(cx, cy, 242.f, 2.f, 267.3f, 5.4f, 0xffb050ff);
    sink.arc(cx, cy, 242.f, 2.f, 87.3f, 5.4f, 0xffb050ff);

    // Ticks: measured centroids of the 13 orange ticks, oriented at
    // (583,335).
    static constexpr float kTicks[13][2] = {
        {416.9f, 446.8f},
        {388.9f, 383.1f},
        {386.9f, 314.8f},
        {407.4f, 247.3f},
        {453.4f, 190.3f},
        {513.5f, 154.5f},
        {582.5f, 138.0f},
        {651.5f, 151.1f},
        {710.8f, 187.8f},
        {759.9f, 247.4f},
        {779.0f, 314.7f},
        {775.8f, 382.9f},
        {749.2f, 447.1f}
    };
    const float tcx = 583.f, tcy = 335.f;
    auto tickAngle = [&](int i) {
      return std::atan2(kTicks[i][1] - tcy, kTicks[i][0] - tcx) *
             57.29578f;
    };
    for (int i = 0; i < 12; ++i) {
      float a0 = tickAngle(i), a1 = tickAngle(i + 1);
      if (a1 < a0) a1 += 360.f;
      for (int m = 1; m < 5; ++m) {
        const float deg = a0 + (a1 - a0) * static_cast<float>(m) / 5.f;
        const float r = 199.f;
        sink.pushRotate(deg + 90.f, at.x(tcx), at.y(tcy));
        sink.fill(
            {at.x(tcx) - 0.75f, at.y(tcy) - r - 6.f, 1.5f, 12.f},
            0x71625bff
        );
        sink.pop();
      }
    }
    for (int i = 0; i < 13; ++i) {
      const float deg = tickAngle(i);
      const float r =
          std::hypot(kTicks[i][0] - tcx, kTicks[i][1] - tcy);
      const float len = (i == 0 || i == 12) ? 18.f : 14.f;
      sink.pushRotate(deg + 90.f, at.x(tcx), at.y(tcy));
      sink.fillRounded(
          {at.x(tcx) - 1.75f, at.y(tcy) - r - len * 0.5f, 3.5f, len},
          1,
          Ref::kOrange
      );
      sink.pop();
    }
    // Numerals at measured ink boxes.
    static constexpr Ink kLabels[7] = {
        {434, 423, 444, 442},
        {408, 309, 434, 328},
        {458, 209, 494, 227},
        {565, 168, 602, 186},
        {665, 209, 705, 227},
        {720, 309, 760, 328},
        {694, 423, 734, 442}
    };
    for (int k = 0; k <= 6; ++k) {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "%d", k * 50);
      fit(sink, e, at, buf, kLabels[k], Ref::kNumeral, kNumeralTrack);
    }

    // Needle: tapered in steps from hub to tip, with a soft glow. The
    // reference needle line passes ~5px right of the hub (parallax of
    // the raised needle), so it pivots at (589,330), angle 327.3deg.
    const float nx = at.x(589.f), ny = at.y(330.f);
    const float deg = kStartDeg + kSweepDeg * needleKmh / 300.f;
    // Glow: one continuous additive stroke; discrete glow discs read as
    // beads.
    {
      const float c = std::cos(deg * 0.0174533f),
                  s = std::sin(deg * 0.0174533f);
      const std::vector<float> glow{
          nx + 34.f * c, ny + 34.f * s, nx + 190.f * c, ny + 190.f * s
      };
      sink.pushBlend(BlendMode::Plus);
      sink.polyline(glow.data(), 2, 14.f, 0.f, 0xfa780814u);
      sink.polyline(glow.data(), 2, 7.f, 0.f, 0xfa780820u);
      sink.pop();
    }
    sink.pushRotate(deg + 90.f, nx, ny);
    constexpr int kSteps = 16;
    for (int s = 0; s < kSteps; ++s) {
      const float r0 =
          15.f + (196.f - 15.f) * static_cast<float>(s) / kSteps;
      const float r1 =
          15.f + (196.f - 15.f) * static_cast<float>(s + 1) / kSteps;
      const float w = std::max(
          1.f, 12.f - 11.f * (static_cast<float>(s) + 0.5f) / kSteps
      );
      sink.fill(
          {nx - w * 0.5f, ny - r1, w, r1 - r0 + 0.5f}, 0xff7d12ff
      );
    }
    sink.pop();
    // Hub: dark disc with copper-to-grey ring band (probe row 330).
    const float hx = at.x(583.5f), hy = at.y(332.f);
    sink.fillRounded(
        {hx - 34.f, hy - 34.f, 68.f, 68.f}, 34, 0x0c1217ff
    );
    sink.arc(hx, hy, 30.5f, 2.f, 0.f, 360.f, 0x2b2623ff);
    sink.arc(hx, hy, 25.5f, 2.f, 0.f, 360.f, 0x1e2329ff);
    sink.arc(hx, hy, 22.5f, 1.5f, 0.f, 360.f, 0x33383cff);
    sink.fillRounded(
        {hx - 10.f, hy - 10.f, 20.f, 20.f}, 10, 0x111a20ff
    );

    // Digital readout + target plate.
    paintReadout(sink, e, at);
    fit(sink, e, at, "KM/H", {551, 462, 616, 479}, Ref::kOrange);
    // Plate: small left chamfer, pointed right end (probe col cuts).
    sink.fillChamfer(
        at.box(485.f, 494.f, 197.f, 29.f), 10.f, 0x0b1014ff
    );
    path(
        sink,
        at.pts(
            {496.f,
             494.f,
             660.f,
             494.f,
             682.f,
             508.5f,
             660.f,
             523.f,
             496.f,
             523.f,
             484.f,
             508.5f,
             496.f,
             494.f}
        ),
        1.f,
        0x1e242aff
    );
    char tgt[32];
    std::snprintf(tgt, sizeof(tgt), "TARGET %d KM/H", targetKmh);
    fit(sink, e, at, tgt, {510, 504, 657, 516}, Ref::kOrangeText);
  }
};

template <class... As>
Block speedGauge(SpeedGauge g, As&&... as) {
  configureTags(g, std::forward<As>(as)...);
  return finish(std::move(g), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
