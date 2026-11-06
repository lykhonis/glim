#pragma once

#include <cstdint>
#include <string>

#include "Card.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

// Labeled text field, composed: the card paints the chrome, the native
// view shows live text on top; the tree owns geometry and focus, never
// pixels.
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
