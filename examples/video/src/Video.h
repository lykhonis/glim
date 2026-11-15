#pragma once

#include <cstdint>
#include <utility>

#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct VideoView {
  std::uint32_t image = 0;
  State<bool> playing;
};

struct VideoPlate {
  static constexpr const char* kKind = "VideoPlate";
  std::uint32_t image = 0;
  const char* kind() const { return kKind; }
  Box place(Constraints c, const Engine&) const {
    const float w = isUnbounded(c.maxW) ? 640.0f : c.maxW;
    return detail::resolveSize(w, w * 9.0f / 16.0f, c);
  }
  void record(const Box& box, Rec& rec) const { rec.push(kKind, box); }
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
    lines("Tap the video to play or pause.")
  );
}

}  // namespace toolkit
}  // namespace glim
