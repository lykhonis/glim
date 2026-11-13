#pragma once

// Backdrop: the villa photographs, cover-cropped to the window, under a
// theme veil. Night shows the house dark, the lit photo fades in with
// the lights, and day fades over both with the theme. The wash shows
// through only when the photos are not loaded. Display only; it does
// not scroll.

#include <algorithm>

#include "HomeTheme.h"
#include "Villa.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Backdrop {
  static constexpr const char* kKind = "Backdrop";
  float daylight = 1.0f;  // 0 night .. 1 day
  float lit = 0.0f;       // 0 house dark .. 1 every light on

  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(
        isUnbounded(c.maxW) ? 0.0f : c.maxW,
        isUnbounded(c.maxH) ? 0.0f : c.maxH,
        c
    );
  }
  void record(const Box& box, Rec& rec) const { rec.push(kKind, box); }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    // First gradient is the theme wash: the theme test reads it, and it
    // shows wherever the photographs have not been registered.
    sink.fillGradient(
        box,
        0,
        themeColor(e, kShowBgTop, Show::kBgTop),
        themeColor(e, kShowBgBottom, Show::kBgBottom)
    );
    VillaPlates plates;
    if (e.store)
      plates = e.store->get().state(kVillaKey, VillaPlates{}).get();
    if (plates.day.id != 0 && plates.night.id != 0) {
      sink.pushClip(box);
      if (daylight < 1.0f) {
        cover(box, plates.night, 1.0f, sink);
        cover(box, plates.lit, lit, sink);
      }
      cover(box, plates.day, daylight, sink);
      sink.pop();
    } else {
      glows(box, sink, e);
    }
    sink.fillGradient(
        box,
        0,
        themeColor(e, kShowScrimTop, Show::kScrimTop),
        themeColor(e, kShowScrimBottom, Show::kScrimBottom)
    );
  }

 private:
  static void cover(
      const Box& box,
      const VillaPlate& plate,
      float alpha,
      AnySink& sink
  ) {
    if (plate.id == 0 || plate.w <= 0 || plate.h <= 0 || alpha <= 0.0f)
      return;
    const float scale = std::max(
        box.w / static_cast<float>(plate.w),
        box.h / static_cast<float>(plate.h)
    );
    const float dw = static_cast<float>(plate.w) * scale;
    const float dh = static_cast<float>(plate.h) * scale;
    sink.pushOpacity(alpha);
    sink.image(
        {box.x + (box.w - dw) * 0.5f,
         box.y + (box.h - dh) * 0.5f,
         dw,
         dh},
        plate.id,
        false
    );
    sink.pop();
  }

  static void glows(const Box& box, AnySink& sink, const Engine& e) {
    const float r = std::max(box.w, box.h);
    auto glow = [&](float cx, float cy, std::uint32_t rgba) {
      sink.fillRadial(
          {cx - r, cy - r, 2.0f * r, 2.0f * r},
          0,
          rgba,
          rgba & 0xffffff00u
      );
    };
    glow(
        box.x + box.w,
        box.y + box.h,
        themeColor(e, kShowGlowWarm, Show::kGlowWarm)
    );
    glow(box.x, box.y, themeColor(e, kShowGlowCool, Show::kGlowCool));
  }
};

struct BackdropProps {
  float daylight = 1.0f;
  float lit = 0.0f;
};

inline Block backdrop(BackdropProps p = {}) {
  return finish(Backdrop{p.daylight, p.lit});
}

}  // namespace toolkit
}  // namespace glim
