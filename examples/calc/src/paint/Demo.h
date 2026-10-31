#pragma once

// Paint-demo tree shared by the --paint preview and the bridge tests.
// Same composition machinery as calcTree/modalTree plus Veil/Video and
// a glass tag: scrim + foreign-video plate under a frosted card.
// The seat strip is painted explicitly after the tree (sibling, never
// inside glass) — the Seat element itself lands in T5.

#include <utility>

#include "elements/Button.h"
#include "elements/Card.h"
#include "elements/Veil.h"
#include "elements/Video.h"
#include "glim/toolkit/Toolkit.h"
#include "paint/Bridge.h"
#include "paint/VideoFeed.h"

namespace glim {
namespace toolkit {
namespace paint {

inline Block paintDemoTree(Action onNo, Action onYes) {
  Block dialog = card(
      glassClear(),
      radius(6),
      gap(8),
      lines("Delete?"),
      row(gap(8),
          button("No", std::move(onNo)),
          button("Yes", std::move(onYes)))
  );
  return stack(
      veil(),
      video(),
      column(
          gap(8),
          empty(expand()),
          row(gap(0),
              empty(expand()),
              std::move(dialog),
              empty(expand())),
          empty(expand())
      )
  );
}

inline constexpr std::uint32_t kDemoSeatId = 7;

inline Box demoSeatBox() { return {16, 424, 688, 40}; }

// Shared paint entry: tree + seat sibling + focus ring overlay.
// Sink-agnostic — ContextSink paints pixels, OpsRecorder records the
// serializable UI ops for the archive/network path.
template <class S>
void paintDemoInto(
    const Block& demo, const Box& box, S& sink, VideoFeed& feed
) {
  Engine e;
  paintTree(demo, box, e, sink, feed);
  sink.slot(demoSeatBox(), kDemoSeatId);
  paintFocusRing(demo, box, 0, sink);
}

}  // namespace paint
}  // namespace toolkit
}  // namespace glim
