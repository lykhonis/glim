#pragma once

// Showcase paint entry shared by the windowed run and the golden test.
// Screen first, fullscreen modal overlay second, seat strip as a
// sibling after (never inside glass), focus ring last. Sink-agnostic.
#include "glim/toolkit/Toolkit.h"
#include "paint/Bridge.h"
#include "paint/VideoFeed.h"

namespace glim {
namespace toolkit {
namespace paint {

inline constexpr std::uint32_t kShowcaseSeatId = 7;

inline Box showcaseSeatBox() { return {16, 424, 688, 40}; }

template <class S>
void paintShowcaseInto(
    const Block& screen,
    const Block& modal,
    const Box& box,
    S& sink,
    VideoFeed& feed
) {
  Engine e;
  paintTree(screen, box, e, sink, feed);
  paintTree(modal, box, e, sink, feed);
  sink.slot(showcaseSeatBox(), kShowcaseSeatId);
  paintFocusRing(screen, box, 0, sink);
}

}  // namespace paint
}  // namespace toolkit
}  // namespace glim
