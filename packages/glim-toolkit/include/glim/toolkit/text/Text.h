#pragma once

// Text measure → break → layout; per-glyph raster stays paint-side.
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

// Fixed-advance offsets by design; engine only selects the breaker.
std::vector<PlacedLine> layoutPlacedLines(
  const std::string& text, const Box& box, bool wrap, int align, const Engine& e = Engine{}
);

}  // namespace text
}  // namespace toolkit
}  // namespace glim
