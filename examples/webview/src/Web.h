#pragma once

#include <cstdint>
#include <utility>

#include "glim/math.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct WebScreen {
  const char* mode = "";
  glim::Vec2 size{};
  // Slot reserves the seat; Texture samples the image.
  bool slot = false;
  std::uint32_t seatId = 0;
  std::uint32_t image = 0;
  LayerInputFn input;
};

inline Block webTitle(const char* mode) {
  return row(
    gap(12),
    row(pad(14, 8), glass({.radius = 14}), lines("Glim WebView")),
    lines(mode)
  );
}

// Texture pixels are Glim's, so the title floats on glass over the
// page. A Slot view composites above the surface and would cover it, so
// the title sits beside the page instead.
inline Block webTree(WebScreen s) {
  constexpr float kPad = 24;
  if (s.slot) {
    return column(
      expand(),
      pad(kPad),
      gap(12),
      webTitle(s.mode),
      seat(
        {.id = s.seatId, .width = s.size.x - 2 * kPad},
        expand(),
        semantic("Web page")
      )
    );
  }
  return stack(
    layer({
      .image = s.image,
      .onInput = std::move(s.input),
      .width = s.size.x,
      .height = s.size.y,
    }),
    column(pad(kPad), webTitle(s.mode))
  );
}

}  // namespace toolkit
}  // namespace glim
