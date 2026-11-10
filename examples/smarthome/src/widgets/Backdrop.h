#pragma once

// Backdrop: the villa photograph, cover-cropped to the window, under a
// theme veil. The wash shows through only when the photo is not loaded.
// Display only; it does not scroll.

#include <algorithm>

#include "HomeTheme.h"
#include "Villa.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Backdrop {
  static constexpr const char* kKind = "Backdrop";
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
    // shows wherever the photograph has not been registered.
    sink.fillGradient(
        box,
        0,
        themeColor(e, kShowBgTop, Show::kBgTop),
        themeColor(e, kShowBgBottom, Show::kBgBottom)
    );
    VillaPlate plate;
    if (e.store)
      plate = e.store->get().state(kVillaKey, VillaPlate{}).get();
    if (plate.id != 0 && plate.w > 0 && plate.h > 0) {
      const float scale = std::max(
          box.w / static_cast<float>(plate.w),
          box.h / static_cast<float>(plate.h)
      );
      const float dw = static_cast<float>(plate.w) * scale;
      const float dh = static_cast<float>(plate.h) * scale;
      sink.pushClip(box);
      sink.image(
          {box.x + (box.w - dw) * 0.5f,
           box.y + (box.h - dh) * 0.5f,
           dw,
           dh},
          plate.id,
          false
      );
      sink.pop();
    } else {
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
    sink.fillGradient(
        box,
        0,
        themeColor(e, kShowScrimTop, Show::kScrimTop),
        themeColor(e, kShowScrimBottom, Show::kScrimBottom)
    );
  }
};

inline Block backdrop() { return finish(Backdrop{}); }

}  // namespace toolkit
}  // namespace glim
