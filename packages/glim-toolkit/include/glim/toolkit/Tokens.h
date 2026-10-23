#pragma once

namespace glim {
namespace toolkit {

// Rung 1: named constants only. No tags index into Tokens yet; factories and
// the text measurer reference these instead of repeating literals, so the
// ramp has one home when tags arrive.
struct Tokens {
  static constexpr float kTypeAdvance = 8.0f;
  static constexpr float kTypeLine = 14.0f;
  static constexpr int kPadCard = 12;
  static constexpr int kButtonPadH = 10;
  static constexpr int kButtonPadV = 7;
  static constexpr int kGapMax = 24;
};

}  // namespace toolkit
}  // namespace glim
