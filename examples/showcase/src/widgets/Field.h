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
// Two adjacent strings — positional would confuse them.
struct FieldRow {
  std::string label;
  std::string hint;
  std::uint32_t id = 0;
};

inline Block fieldRow(FieldRow f) {
  return column(
      gap(4),
      lines(std::move(f.label)),
      card(
          radius(12),
          seat(
              {.id = f.id, .width = 600.0f, .height = 40.0f},
              semantic("field")
          )
      ),
      lines(std::move(f.hint))
  );
}

}  // namespace toolkit
}  // namespace glim
