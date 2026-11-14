#pragma once

// Cluster cards (paQys.jpg): battery, altitude, systems, VSI,
// navigation, environment, mode bar, plus the column chrome. Each card
// paints in reference pixels (fui::At) measured with the probe; values
// come from fields so the readouts stay live. Kinds are stable for
// headless gates.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

#include "../InfoTheme.h"
#include "Fui.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

// ---- Fixed spacer: exact geometry, no paint ----

struct Space {
  static constexpr const char* kKind = "Space";
  const char* kind() const { return kKind; }
  float width = 0.f;
  float height = 0.f;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(width, height, c);
  }
  void record(const Box&, Rec&) const {}
};

template <class... As>
Block space(Space s, As&&... as) {
  configureTags(s, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

namespace fui {

// Frosted floating card: flat neutral frost over the city map, so the
// buildings ghost through without adding hue (UI stays monochrome).
constexpr std::uint32_t kFrostTint = 0x0a0c0e66u;

inline GlassTag cardGlass() {
  return glass(
      {.radius = 30, .edge = GlassEdge::None, .tint = kFrostTint}
  );
}

// Controls: the same frost with a lit rim, so they read as pressable
// without leaving the monochrome palette.
inline GlassTag controlGlass() {
  return glass({.radius = 34, .tint = kFrostTint});
}

// Dial lens: clear glass disc under the gauge; the bezel ring stays
// open so the city refracts through it around the face.
inline GlassTag dialGlass() {
  return glass({.clear = true, .radius = 258});
}

inline void plate(
    AnySink& sink,
    const At& at,
    Box fill,
    float cut,
    std::initializer_list<float> frame
) {
  sink.fillChamfer(
      at.box(fill.x, fill.y, fill.w, fill.h),
      cut,
      withAlpha(Ref::kCard, 0.18f)
  );
  path(sink, at.pts(frame), 1.f, Ref::kFrame);
}

}  // namespace fui

// ---- Column chrome: outer brackets around one card column ----

// Left column frame spans x 0..300, right column x 880..1168 (design
// px).
struct ClusterChrome {
  static constexpr const char* kKind = "ClusterChrome";
  bool right = false;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(right ? 288.f : 300.f, 784.f, c);
  }
  void record(const Box& box, Rec& rec) const { rec.push(kKind, box); }
  void paint(const Box& box, AnySink& sink, const Engine&) const {
    using namespace fui;
    const At at(box, right ? 880.f : 0.f, 0.f);
    if (!right) {
      path(sink, at.pts({7.5f, 150.f, 7.5f, 32.f}), 1.f, Ref::kCopper);
      path(
          sink,
          at.pts({7.5f, 32.f, 30.f, 9.5f, 284.f, 9.5f, 296.f, 21.f}),
          1.f,
          Ref::kBracket
      );
      path(
          sink, at.pts({15.5f, 185.f, 15.5f, 275.f}), 1.f, Ref::kBracket
      );
      path(
          sink, at.pts({13.5f, 540.f, 13.5f, 720.f}), 1.f, Ref::kBracket
      );
      path(
          sink,
          at.pts({60.f, 745.5f, 300.f, 745.5f}),
          1.f,
          Ref::kBracket
      );
      return;
    }
    path(sink, at.pts({885.f, 9.5f, 1138.f, 9.5f}), 1.f, Ref::kBracket);
    path(
        sink,
        at.pts({1138.f, 9.5f, 1160.5f, 31.f, 1160.5f, 100.f}),
        1.f,
        Ref::kCopper
    );
    path(
        sink,
        at.pts({1154.5f, 630.f, 1154.5f, 720.f}),
        1.f,
        Ref::kBracket
    );
    path(
        sink, at.pts({1128.f, 745.5f, 1155.f, 722.f}), 1.f, Ref::kCopper
    );
  }
};

inline Block clusterChrome(bool right) {
  ClusterChrome c;
  c.right = right;
  return finish(std::move(c));
}

// ---- Battery: ring gauge + % + range + segmented bar ----

struct BatteryArc {
  static constexpr const char* kKind = "BatteryArc";
  float level = 0.72f;  // 0..1
  int rangeKm = 412;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(238.f, 273.f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 8);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    using namespace fui;
    const At at(box, 18.f, 21.f);
    plate(
        sink,
        at,
        {19.f, 22.f, 236.f, 271.f},
        17.f,
        {37.f,  21.5f,  237.f, 21.5f,  255.5f, 40.f,  255.5f,
         100.f, 249.5f, 106.f, 249.5f, 280.f,  236.f, 293.5f,
         38.f,  293.5f, 23.5f, 279.f,  23.5f,  160.f, 18.5f,
         155.f, 18.5f,  40.f,  37.f,   21.5f}
    );
    hline(sink, at.x(35.f), at.x(245.f), at.y(60.f), Ref::kRule);
    // Battery glyph: cell outline, nub, charge.
    path(
        sink,
        at.pts(
            {42.5f,
             38.5f,
             62.5f,
             38.5f,
             62.5f,
             49.5f,
             42.5f,
             49.5f,
             42.5f,
             38.5f}
        ),
        1.f,
        Ref::kLabel
    );
    sink.fill(at.box(63.f, 42.f, 2.f, 4.f), Ref::kLabel);
    sink.fill(at.box(45.f, 41.f, 15.f, 6.f), 0x5c6469ff);
    fit(sink, e, at, "BATTERY", {76, 39, 137, 49}, Ref::kLabel);
    // Ring: 272deg sweep from 134.4deg, lit fraction of a full-scale
    // ring.
    const float cx = at.x(134.f), cy = at.y(140.5f);
    sink.arc(cx, cy, 70.5f, 2.f, 0.f, 360.f, 0x24170eff);
    sink.arc(
        cx,
        cy,
        62.5f,
        9.f,
        134.4f,
        272.2f * std::fmin(1.f, level / 0.72f),
        Ref::kOrange
    );
    char pct[8];
    std::snprintf(
        pct, sizeof(pct), "%d", static_cast<int>(level * 100.f + 0.5f)
    );
    fit(sink,
        e,
        at,
        pct,
        {101, 122, 150, 159},
        Ref::kOrange,
        kNumeralTrack);
    fit(sink, e, at, "%", {158, 140, 173, 159}, Ref::kOrange);
    fit(sink, e, at, "EST. RANGE", {93, 206, 171, 215}, Ref::kLabel);
    char rg[16];
    std::snprintf(rg, sizeof(rg), "%d", rangeKm);
    fit(sink, e, at, rg, {98, 226, 132, 241}, Ref::kValue, kNumeralTrack
    );
    fit(sink, e, at, "KM", {142, 229, 162, 241}, Ref::kValue);
    // Bar: framed track, lit run, two dim tail segments.
    sink.stroke(at.box(43.f, 255.f, 186.f, 19.f), 3, 1.f, 0x1f2a31ff);
    sink.fill(
        at.box(47.f, 259.f, 138.f * (level / 0.72f), 11.f), Ref::kOrange
    );
    sink.fill(at.box(187.f, 259.f, 14.f, 11.f), 0x16222aff);
    sink.fill(at.box(204.f, 259.f, 19.f, 11.f), 0x16222aff);
  }
};

template <class... As>
Block batteryArc(BatteryArc b, As&&... as) {
  configureTags(b, std::forward<As>(as)...);
  return finish(std::move(b), std::forward<As>(as)...);
}

// ---- Altitude: value + terrain chart + target ----

struct AltitudeCard {
  static constexpr const char* kKind = "AltitudeCard";
  int altM = 285;
  int targetM = 300;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(236.f, 204.f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 8);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    using namespace fui;
    const At at(box, 22.f, 309.f);
    plate(
        sink,
        at,
        {23.f, 310.f, 234.f, 202.f},
        13.f,
        {36.f,   309.5f, 192.f,  309.5f, 195.f, 312.5f, 246.f, 312.5f,
         257.5f, 324.f,  257.5f, 498.f,  244.f, 512.5f, 34.f,  512.5f,
         22.5f,  500.f,  22.5f,  322.f,  36.f,  309.5f}
    );
    hline(sink, at.x(35.f), at.x(247.f), at.y(344.f), Ref::kRule);
    hline(sink, at.x(36.f), at.x(247.f), at.y(477.f), Ref::kRule);
    fit(sink, e, at, "ALTITUDE", {45, 325, 112, 335}, Ref::kLabel);
    char av[16];
    std::snprintf(av, sizeof(av), "%d", altM);
    fit(sink, e, at, av, {45, 360, 111, 390}, Ref::kValue, kNumeralTrack
    );
    fit(sink, e, at, "M AGL", {124, 380, 163, 390}, Ref::kLabel);
    path(
        sink,
        at.pts(
            {184.5f,
             389.f,
             201.f,
             368.f,
             205.f,
             373.f,
             213.5f,
             360.5f,
             226.f,
             378.f,
             229.5f,
             373.f,
             238.5f,
             389.f,
             184.5f,
             389.f}
        ),
        1.5f,
        Ref::kOrange
    );
    path(sink, at.pts({201.f, 368.f, 212.f, 384.f}), 1.f, Ref::kOrange);
    // Chart frame + grid.
    sink.stroke(at.box(41.f, 406.f, 201.f, 62.f), 0, 1.f, 0x131c22ff);
    for (float gx : {92.f, 122.f, 155.f, 219.f})
      sink.fill(at.box(gx, 407.f, 1.f, 60.f), 0x10191fff);
    hline(sink, at.x(42.f), at.x(241.f), at.y(436.f), 0x10191fff);
    path(
        sink,
        at.pts({43.f,  453.5f, 57.f,  452.f, 68.f,  444.f,
                80.f,  439.5f, 91.f,  446.f, 107.f, 437.f,
                121.f, 443.5f, 131.f, 439.f, 144.f, 429.5f,
                155.f, 422.5f, 170.f, 421.f, 183.f, 434.f,
                192.f, 434.f,  204.f, 441.f, 218.f, 456.f,
                226.f, 452.f,  233.f, 452.f, 240.f, 449.5f}),
        1.6f,
        Ref::kOrange
    );
    char tgt[32];
    std::snprintf(tgt, sizeof(tgt), "TARGET ALT %d M", targetM);
    fit(sink, e, at, tgt, {43, 489, 167, 498}, Ref::kLabel);
  }
};

template <class... As>
Block altitudeCard(AltitudeCard a, As&&... as) {
  configureTags(a, std::forward<As>(as)...);
  return finish(std::move(a), std::forward<As>(as)...);
}

// ---- Systems rows (keeps SysStatus kind for gates) ----

struct SysStatus {
  static constexpr const char* kKind = "SysStatus";
  int states[4] = {0, 0, 0, 0};  // all nominal
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(283.f, 193.f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 4);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    using namespace fui;
    const At at(box, 21.f, 529.f);
    plate(
        sink,
        at,
        {22.f, 530.f, 281.f, 191.f},
        15.f,
        {33.f,   529.5f, 231.f,  529.5f, 235.f, 532.5f, 283.f, 532.5f,
         303.5f, 552.f,  303.5f, 705.f,  289.f, 721.5f, 40.f,  721.5f,
         21.5f,  705.f,  21.5f,  541.f,  33.f,  529.5f}
    );
    for (float ry : {566.f, 600.f, 633.f, 667.f})
      hline(sink, at.x(33.f), at.x(292.f), at.y(ry), Ref::kRule);
    path(
        sink,
        at.pts(
            {48.5f,
             540.5f,
             57.f,
             544.f,
             57.f,
             551.f,
             48.5f,
             559.f,
             40.f,
             551.f,
             40.f,
             544.f,
             48.5f,
             540.5f}
        ),
        1.2f,
        Ref::kLabel
    );
    path(
        sink,
        at.pts({44.5f, 550.f, 47.5f, 553.f, 53.f, 546.5f}),
        1.2f,
        Ref::kLabel
    );
    fit(sink, e, at, "SYSTEMS", {71, 545, 133, 555}, Ref::kLabel);
    struct Row {
      const char* name;
      Ink ink;
      bool lit;
    };
    const Row rows[4] = {
        {"POWER", {42, 579, 82, 588}, false},
        {"PROPULSION", {42, 612, 112, 621}, true},
        {"AVIONICS", {42, 646, 96, 655}, false},
        {"SENSORS", {42, 680, 95, 689}, true}
    };
    const float segX[5][2] = {
        {124, 141}, {143, 160}, {162, 180}, {181, 199}, {201, 220}
    };
    for (const Row& r : rows) {
      fit(sink, e, at, r.name, r.ink, Ref::kLabel);
      for (int s = 0; s < 5; ++s) {
        const std::uint32_t c = s == 4              ? Ref::kSegmentDim
                                : (s == 3 && r.lit) ? 0xea730fff
                                                    : Ref::kSegment;
        sink.fill(
            at.box(
                segX[s][0],
                r.ink.y0,
                segX[s][1] - segX[s][0] + 1.f,
                11.f
            ),
            c
        );
      }
      sink.fillRounded(
          at.box(232.5f, r.ink.y0 + 1.5f, 6.f, 6.f), 3, Ref::kOrange
      );
      fit(sink,
          e,
          at,
          "NOMINAL",
          {248, r.ink.y0 + 1.f, 283, r.ink.y0 + 8.f},
          Ref::kOrangeText);
    }
  }
};

template <class... As>
Block sysStatus(SysStatus s, As&&... as) {
  configureTags(s, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

// ---- VSI: dual tick scale, segmented column, glowing marker, value
// ----

struct Vsi {
  static constexpr const char* kKind = "VSI";
  float rate = 0.23f;  // -1..1 -> -10..+10 m/s
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(237.f, 303.f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 6);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    using namespace fui;
    const At at(box, 914.f, 21.f);
    plate(
        sink,
        at,
        {915.f, 22.f, 235.f, 301.f},
        17.f,
        {932.f, 21.5f,   1132.f, 21.5f,   1150.5f, 40.f,   1150.5f,
         132.f, 1145.5f, 138.f,  1145.5f, 305.f,   1131.f, 323.5f,
         937.f, 323.5f,  918.5f, 307.f,   918.5f,  106.f,  914.5f,
         102.f, 914.5f,  40.f,   932.f,   21.5f}
    );
    hline(sink, at.x(930.f), at.x(1135.f), at.y(61.f), 0x111a20ff);
    // Up-arrow glyph.
    path(sink, at.pts({945.f, 38.f, 945.f, 53.f}), 1.2f, Ref::kLabel);
    path(
        sink,
        at.pts({940.5f, 43.f, 945.f, 37.5f, 949.5f, 43.f}),
        1.2f,
        Ref::kLabel
    );
    path(sink, at.pts({941.f, 53.5f, 949.f, 53.5f}), 1.2f, Ref::kLabel);
    fit(sink,
        e,
        at,
        "VERTICAL SPEED (VSI)",
        {963, 40, 1114, 50},
        Ref::kLabel);
    // Scale: majors every 44px from 86.5, four minors between.
    for (int i = 0; i <= 20; ++i) {
      const float y = 86.5f + 44.15f * static_cast<float>(i) / 5.f;
      const bool major = i % 5 == 0;
      const std::uint32_t c = major ? 0xad7748ff : 0x50352eff;
      sink.fill(
          at.box(
              major ? 983.f : 986.f, y - 1.f, major ? 13.f : 10.f, 2.f
          ),
          c
      );
      sink.fill(at.box(1051.f, y - 1.f, major ? 12.f : 10.f, 2.f), c);
    }
    const Type num{11.f, 0.f, 1.f, 0.f, 0xc8cacdff};
    fit(sink, e, at, "+10", {952, 79, 974, 89}, num.color);
    fit(sink, e, at, "-10", {954, 256, 974, 266}, num.color);
    fit(sink, e, at, "+10", {1071, 79, 1092, 89}, num.color);
    fit(sink, e, at, "m/s", {1071, 94, 1091, 103}, num.color);
    fit(sink, e, at, "+5", {1071, 125, 1086, 135}, num.color);
    fit(sink, e, at, "0", {1072, 169, 1078, 179}, num.color);
    fit(sink, e, at, "-5", {1072, 213, 1084, 224}, num.color);
    fit(sink, e, at, "-10", {1071, 256, 1091, 266}, num.color);
    // Column: dark segments, lit run from the marker down, marker glow.
    const float top = 78.f, bottom = 267.f, zero = 175.5f;
    sink.fill(at.box(1013.f, top, 23.f, bottom - top), 0x0e161bff);
    for (float y = top; y < bottom; y += 9.f)
      sink.fill(at.box(1014.f, y, 21.f, 8.f), 0x152027ff);
    // Reference draws +2.3 m/s 5px above zero (not on the tick scale).
    constexpr float kMarkerPxPerRate = 21.7f;
    const float markY = zero - rate * kMarkerPxPerRate;
    for (float y = markY + 4.f; y < bottom - 8.f; y += 9.f) {
      const float t = (y - markY) / (bottom - markY);
      const std::uint32_t c = t < 0.35f  ? 0xc54c07ff
                              : t < 0.7f ? 0x9a3f08ff
                                         : 0x69310eff;
      sink.fill(at.box(1014.f, y, 21.f, 8.f), c);
    }
    sink.pushEffect(
        at.box(1003.f, markY - 9.f, 42.f, 18.f),
        8,
        static_cast<std::uint32_t>(EffectKind::Glow),
        Ref::kOrange,
        0.3f
    );
    sink.pop();
    sink.fill(at.box(1008.f, markY - 3.5f, 32.f, 8.f), 0xfea330ff);
    char vb[16];
    std::snprintf(
        vb, sizeof(vb), "%+.1f", static_cast<double>(rate * 10.f)
    );
    fit(sink,
        e,
        at,
        vb,
        {986, 284, 1053, 309},
        Ref::kOrange,
        kNumeralTrack);
    fit(sink, e, at, "m/s", {1065, 298, 1091, 309}, 0xb47a44ff);
  }
};

template <class... As>
Block vsi(Vsi v, As&&... as) {
  configureTags(v, std::forward<As>(as)...);
  return finish(std::move(v), std::forward<As>(as)...);
}

// ---- Navigation ----

struct NavCard {
  static constexpr const char* kKind = "NavCard";
  float distanceKm = 12.4f;
  int headingDeg = 128;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(233.f, 179.f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 8);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    using namespace fui;
    const At at(box, 913.f, 339.f);
    plate(
        sink,
        at,
        {914.f, 340.f, 231.f, 177.f},
        13.f,
        {925.f,
         339.5f,
         1132.f,
         339.5f,
         1145.5f,
         353.f,
         1145.5f,
         505.f,
         1132.f,
         517.5f,
         925.f,
         517.5f,
         913.5f,
         505.f,
         913.5f,
         352.f,
         925.f,
         339.5f}
    );
    hline(sink, at.x(926.f), at.x(1134.f), at.y(375.f), Ref::kRule);
    fit(sink, e, at, "NAVIGATION", {938, 355, 1023, 365}, Ref::kLabel);
    fit(sink, e, at, "WAYPOINT", {938, 387, 1004, 397}, Ref::kLabel);
    fit(sink, e, at, "ALPHA-7", {938, 406, 1011, 418}, Ref::kOrangeText
    );
    fit(sink, e, at, "DISTANCE", {939, 439, 1001, 448}, Ref::kLabel);
    char dist[16];
    std::snprintf(
        dist, sizeof(dist), "%.1f", static_cast<double>(distanceKm)
    );
    fit(sink,
        e,
        at,
        dist,
        {939, 458, 977, 472},
        Ref::kValue,
        kNumeralTrack);
    fit(sink, e, at, "KM", {986, 462, 1005, 472}, Ref::kValue);
    char hdg[16];
    std::snprintf(hdg, sizeof(hdg), "HDG %d", headingDeg);
    fit(sink, e, at, hdg, {938, 491, 1004, 503}, Ref::kOrangeText);
    sink.arc(
        at.x(1010.5f),
        at.y(493.5f),
        2.3f,
        1.3f,
        0.f,
        360.f,
        Ref::kOrangeText
    );
    path(
        sink,
        at.pts(
            {1101.f,
             489.5f,
             1083.5f,
             497.f,
             1092.f,
             498.5f,
             1094.f,
             506.f,
             1101.f,
             489.5f}
        ),
        1.2f,
        Ref::kLabel
    );
  }
};

template <class... As>
Block navCard(NavCard n, As&&... as) {
  configureTags(n, std::forward<As>(as)...);
  return finish(std::move(n), std::forward<As>(as)...);
}

// ---- Environment ----

struct EnvCard {
  static constexpr const char* kKind = "EnvCard";
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(250.f, 198.f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 8);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    using namespace fui;
    const At at(box, 895.f, 532.f);
    plate(
        sink,
        at,
        {896.f, 533.f, 248.f, 196.f},
        13.f,
        {909.f,
         532.5f,
         1132.f,
         532.5f,
         1144.5f,
         546.f,
         1144.5f,
         716.f,
         1133.f,
         729.5f,
         909.f,
         729.5f,
         895.5f,
         716.f,
         895.5f,
         547.f,
         909.f,
         532.5f}
    );
    hline(sink, at.x(909.f), at.x(1132.f), at.y(568.f), Ref::kRule);
    // Title glyph: gauge lines.
    path(sink, at.pts({924.f, 552.f, 934.f, 552.f}), 1.2f, Ref::kLabel);
    path(sink, at.pts({926.f, 556.f, 938.f, 556.f}), 1.2f, Ref::kLabel);
    path(
        sink, at.pts({928.f, 559.f, 938.f, 545.5f}), 1.2f, Ref::kLabel
    );
    fit(sink, e, at, "ENVIRONMENT", {955, 548, 1056, 558}, Ref::kLabel);
    // Wind glyph.
    path(
        sink, at.pts({916.f, 589.5f, 936.f, 589.5f}), 1.6f, Ref::kOrange
    );
    path(
        sink, at.pts({916.f, 594.5f, 941.f, 594.5f}), 1.6f, Ref::kOrange
    );
    path(
        sink, at.pts({919.f, 599.5f, 934.f, 599.5f}), 1.6f, Ref::kOrange
    );
    sink.arc(
        at.x(937.f), at.y(586.5f), 3.f, 1.4f, 180.f, 270.f, Ref::kOrange
    );
    sink.arc(
        at.x(934.f), at.y(602.5f), 3.f, 1.4f, 270.f, 220.f, Ref::kOrange
    );
    fit(sink, e, at, "WIND", {958, 580, 990, 589}, Ref::kLabel);
    fit(sink, e, at, "6 m/s NE", {959, 598, 1018, 610}, Ref::kValue);
    // Thermometer glyph.
    sink.stroke(
        at.box(926.5f, 631.f, 7.f, 21.f), 3, 1.4f, Ref::kOrange
    );
    sink.fillRounded(
        at.box(924.5f, 648.f, 11.f, 11.f), 5, Ref::kOrange
    );
    sink.fill(at.box(929.f, 640.f, 2.f, 10.f), Ref::kOrange);
    for (float ty : {634.f, 638.f, 642.f})
      sink.fill(at.box(934.f, ty, 3.f, 1.f), Ref::kOrange);
    fit(sink, e, at, "TEMP", {958, 631, 991, 640}, Ref::kLabel);
    fit(sink,
        e,
        at,
        "18",
        {959, 649, 975, 661},
        Ref::kValue,
        kNumeralTrack);
    sink.arc(
        at.x(984.f), at.y(651.5f), 2.f, 1.2f, 0.f, 360.f, Ref::kValue
    );
    fit(sink, e, at, "C", {989, 649, 997, 661}, Ref::kValue);
    // Visibility glyph: dial.
    sink.arc(
        at.x(930.5f), at.y(696.5f), 12.f, 1.3f, 0.f, 360.f, Ref::kLabel
    );
    sink.arc(
        at.x(930.5f), at.y(696.5f), 6.f, 1.2f, 180.f, 180.f, Ref::kLabel
    );
    sink.fillRounded(at.box(929.f, 695.f, 3.f, 3.f), 1, Ref::kOrange);
    fit(sink, e, at, "VISIBILITY", {958, 683, 1022, 692}, Ref::kLabel);
    fit(sink, e, at, "GOOD", {959, 700, 1001, 712}, Ref::kValue);
  }
};

template <class... As>
Block envCard(EnvCard n, As&&... as) {
  configureTags(n, std::forward<As>(as)...);
  return finish(std::move(n), std::forward<As>(as)...);
}

// ---- Mode bar ----

struct ModeBar {
  static constexpr const char* kKind = "ModeBar";
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(543.f, 71.f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 10);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    using namespace fui;
    const At at(box, 323.f, 663.f);
    sink.fillChamfer(
        at.box(324.f, 664.f, 541.f, 69.f),
        30.f,
        withAlpha(Ref::kCard, 0.18f)
    );
    path(
        sink,
        at.pts(
            {323.5f,
             698.f,
             350.f,
             663.5f,
             838.f,
             663.5f,
             865.5f,
             698.f,
             838.f,
             733.5f,
             350.f,
             733.5f,
             323.5f,
             698.f}
        ),
        1.f,
        0x2a3036ff
    );
    for (float dx : {434.f, 547.f, 659.f, 722.f})
      sink.fill(at.box(dx, 681.f, 1.f, 37.f), Ref::kRule);
    fit(sink, e, at, "AUTO", {377, 686, 406, 695}, Ref::kLabel);
    fit(sink, e, at, "NAV", {379, 705, 405, 715}, Ref::kValue);
    fit(sink, e, at, "FLIGHT MODE", {454, 687, 528, 695}, Ref::kLabel);
    fit(sink, e, at, "CRUISE", {465, 705, 513, 715}, Ref::kValue);
    fit(sink, e, at, "STABILITY", {570, 686, 627, 695}, Ref::kLabel);
    fit(sink, e, at, "ENGAGED", {569, 706, 629, 715}, Ref::kOrangeText);
    fit(sink, e, at, "SAFETY", {778, 687, 820, 695}, Ref::kLabel);
    fit(sink, e, at, "ACTIVE", {778, 706, 821, 715}, Ref::kValue);
    // Cube glyph.
    path(
        sink,
        at.pts(
            {689.f,
             684.5f,
             703.5f,
             692.5f,
             703.5f,
             709.f,
             689.f,
             717.f,
             674.5f,
             709.f,
             674.5f,
             692.5f,
             689.f,
             684.5f}
        ),
        1.1f,
        Ref::kLabel
    );
    path(
        sink,
        at.pts({674.5f, 692.5f, 689.f, 700.5f, 703.5f, 692.5f}),
        1.1f,
        Ref::kLabel
    );
    path(
        sink, at.pts({689.f, 700.5f, 689.f, 717.f}), 1.1f, Ref::kLabel
    );
    // Shield glyph.
    path(
        sink,
        at.pts(
            {751.5f,
             688.5f,
             763.f,
             693.f,
             763.f,
             702.f,
             751.5f,
             715.f,
             740.f,
             702.f,
             740.f,
             693.f,
             751.5f,
             688.5f}
        ),
        1.4f,
        Ref::kOrange
    );
    path(
        sink,
        at.pts({746.f, 697.f, 751.5f, 702.5f, 757.f, 696.5f}),
        1.3f,
        Ref::kOrange
    );
    path(
        sink,
        at.pts({751.5f, 702.5f, 751.5f, 709.f}),
        1.3f,
        Ref::kOrange
    );
  }
};

template <class... As>
Block modeBar(ModeBar m, As&&... as) {
  configureTags(m, std::forward<As>(as)...);
  return finish(std::move(m), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
