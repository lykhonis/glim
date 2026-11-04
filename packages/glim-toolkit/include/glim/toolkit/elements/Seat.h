#pragma once

#include <cstdint>
#include <utility>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Event.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

// Toolkit-side reservation for a host native (IME field, picker,
// player view). Boundary: `Slot` is the Graphics/Host mechanism
// (`SlotHole` Shape + native view); `Seat` is the toolkit piece
// reserving the rect. The paint bridge maps the arm to a structural
// slot op (never an entry); `syncSeats` diffs `collectSlots` against
// the attached set per frame (attach/position/detach, position-only
// on move).
//
// IME path: the laid-out `place` Box doubles as the candidate
// `imeAnchor`. Committed text lives in field `State`s; in-progress
// composition stays in the native view. The candidate window is an
// app-owned sibling — the toolkit only places its anchor, never the
// window itself.
struct Seat : WithSemantic {
  static constexpr const char* kKind = "Seat";
  const char* kind() const { return kKind; }
  std::uint32_t id = 0;
  float width = 0;
  float height = 0;
  Box place(Constraints c, const Engine&) const { return detail::resolveSize(width, height, c); }
  // Focusable anchor: tap claims focus for the native field; key
  // handling itself lives in the native view.
  bool handle(const Event& ev) const { return ev.is<PointerDown>() || ev.is<Activate>(); }
};

template <class... As>
auto seat(Seat s, As&&... as) {
  configureTags(s, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
