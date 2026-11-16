#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct VideoView {
  std::uint32_t image = 0;
  State<bool> playing;
  std::function<void()> share;
  std::string shared;
};

// What the clip shows, in its own 640x360 space; grafted under the
// plate.
inline A11yExport clipA11y() {
  return {
    .width = 640,
    .height = 360,
    .nodes =
      {
        {
          .kind = "Clip",
          .label = "Sunset over a lake",
          .box = {0, 0, 640, 360},
        },
        {
          .kind = "Clip",
          .label = "Sun",
          .box = {420, 125, 30, 30},
          .parent = 0,
        },
        {
          .kind = "Clip",
          .label = "Hills",
          .box = {0, 195, 640, 75},
          .parent = 0,
        },
        {
          .kind = "Clip",
          .label = "Lake",
          .box = {0, 270, 640, 90},
          .parent = 0,
        },
      },
  };
}

struct VideoPlate {
  static constexpr const char* kKind = "VideoPlate";
  std::uint32_t image = 0;
  const char* kind() const { return kKind; }
  Box place(Constraints c, const Engine&) const {
    const float w = isUnbounded(c.maxW) ? 640.0f : c.maxW;
    return detail::resolveSize(w, w * 9.0f / 16.0f, c);
  }
  void record(const Box& box, Rec& rec) const { rec.push(kKind, box); }
  A11yExport a11yExport() const {
    return image == 0 ? A11yExport{} : clipA11y();
  }
  void paint(const Box& box, AnySink& sink, const Engine&) const {
    if (image == 0) {
      sink.fill(box, 0x101418ffu);
      return;
    }
    sink.pushClip(box);
    sink.image(box, image, true);
    sink.pop();
  }
};

inline Block videoPlate(VideoPlate p) { return finish(std::move(p)); }

inline Block videoTree(VideoView view) {
  State<bool> playing = view.playing;
  const bool on = *playing;
  return column(
    expand(),
    pad(32),
    gap(16),
    tap(
      stack(
        videoPlate({.image = view.image}),
        column(
          pad(16),
          row(
            pad(14, 8),
            glass({.radius = 14}),
            lines(on ? "Live" : "Paused")
          )
        )
      ),
      [playing]() mutable { playing = !*playing; },
      semantic("Video"),
      a11y({.value = on ? "Playing" : "Paused"})
    ),
    row(
      gap(16),
      tap(
        row(pad(14, 8), glass({.radius = 14}), lines("Share")),
        [share = view.share] {
          if (share) share();
        },
        semantic("Share")
      ),
      lines(
        view.shared.empty() ? "Tap the video to play or pause."
                            : view.shared
      )
    )
  );
}

}  // namespace toolkit
}  // namespace glim
