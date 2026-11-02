#pragma once

#include <cstdint>
#include <string>

#include "Card.h"
#include "glim/toolkit/Block.h"

namespace glim {
namespace toolkit {

// Labeled text field (T8 catalog, composed — no struct, no core
// changes): the section header names the control, the row carries the
// instance label, a rounded card holding the Seat hole, and a hint
// line. The card paints the field chrome (visible in every
// renderer); the native view (syncSeats) shows the live text on top;
// the toolkit tree owns geometry and focus, never pixels.
// Committed text syncs back in a later slice; the hint documents what
// belongs there meanwhile.
inline Block fieldRow(
    std::string label, std::string hint, std::uint32_t id
) {
  return column(
      gap(4),
      lines(std::move(label)),
      card(radius(12), seat(id, 664.0f, 40.0f, semantic("field"))),
      lines(std::move(hint))
  );
}

}  // namespace toolkit
}  // namespace glim
