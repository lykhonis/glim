#pragma once

// Text measure → break → layout, inside the toolkit until a second
// consumer appears (then promote to glim-text). Boundary: measure
// (advances) → break (wrap/truncate) → layout (placed boxes).
// Custom advances publish via the Measurer env (Measurer-style);
// per-glyph raster stays paint-side, this module only places boxes.
// Align offsets stay fixed-advance even when the Measurer sized the
// box (see layoutPlacedLines).

#include <string>
#include <vector>

#include "glim/toolkit/Layout.h"
#include "glim/toolkit/Measure.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {
namespace text {

struct PlacedLine {
  Box box;
  std::string text;
};

Box measureText(const std::string& text, Constraints c, const Engine& e);

std::vector<std::string> wrapText(const std::string& text, float maxW, const Engine& e = Engine{});

std::vector<std::string> truncateLines(std::vector<std::string> lines, float maxW, float maxH);

std::vector<std::string> layoutLines(
    const std::string& text, float maxW, float maxH, bool wrap, const Engine& e = Engine{}
);

// Visual lines for an allotted box, with per-line align offsets.
// Fixed-advance offsets by design (see header note); the engine only
// selects the breaker, never the offset metric.
std::vector<PlacedLine> layoutPlacedLines(
    const std::string& text, const Box& box, bool wrap, int align, const Engine& e = Engine{}
);

}  // namespace text
}  // namespace toolkit
}  // namespace glim
