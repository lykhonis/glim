#pragma once

namespace glim {
namespace toolkit {

// Text measurement service (the §9 Shaper seam, first rung). Layout is
// pure and paint-free, so it cannot see the font atlas — but the
// embedder can. Whoever owns pixels (the paint bridge, in practice)
// publishes a measurer into the Engine env; text::measureText uses
// it when present and falls back to the fixed advance otherwise.
// Fallback keeps every headless test exact and deterministic; the
// windowed and golden runs measure for real, so buttons fit their
// labels instead of the advance guess.
struct Measurer {
  float (*measure)(const char* text, float sizePx) = nullptr;
};

}  // namespace toolkit
}  // namespace glim
