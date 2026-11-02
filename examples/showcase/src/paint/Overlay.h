#pragma once

// Showcase paint entry shared by the windowed run and the golden test.
// Screen tree owns its slot (Seat in the tree); focus ring last.
// Sink-agnostic.
#include "glim/toolkit/Toolkit.h"
#include "paint/Bridge.h"
#include "paint/VideoFeed.h"

namespace glim {
namespace toolkit {
namespace paint {

inline constexpr std::uint32_t kShowcaseSeatId = 7;

template <class S>
void paintShowcaseInto(
    const Block& screen,
    const Box& box,
    S& sink,
    VideoFeed& feed,
    int focus = 0,
    const Engine& e = Engine{},
    bool focusVisible = true
) {
  paintTree(screen, box, e, sink, feed);
  paintFocusRing(screen, box, focus, sink, e, focusVisible);
}

}  // namespace paint
}  // namespace toolkit
}  // namespace glim
