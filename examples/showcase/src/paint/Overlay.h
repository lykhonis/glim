#pragma once

// Showcase paint entry shared by the windowed run and the golden test.
// Screen tree, seat strip as a sibling after (never inside glass),
// focus ring last. Sink-agnostic.
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
    const Box& box,
    S& sink,
    VideoFeed& feed,
    int focus = 0
) {
  Engine e;
  paintTree(screen, box, e, sink, feed);
  sink.slot(showcaseSeatBox(), kShowcaseSeatId);
  paintFocusRing(screen, box, focus, sink);
}

}  // namespace paint
}  // namespace toolkit
}  // namespace glim
