#pragma once

// Deterministic synthetic series for the dashboard. Pure function of
// (index, range, amplitude): no clock, so golden snapshots are stable
// and interactions (segmented/slider) drive it explicitly.

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace glim {
namespace toolkit {

inline float syntheticHash(int i, int range) {
  std::uint32_t h =
      static_cast<std::uint32_t>(i * 2654435761u ^ range * 40503u);
  h ^= h >> 15;
  h *= 2246822519u;
  h ^= h >> 13;
  return static_cast<float>(h % 1000) / 1000.0f;
}

inline float syntheticValue(
    int i, int range, float level, float phase = 0.f
) {
  const float wave =
      0.5f + 0.5f * std::sin(
                        phase + static_cast<float>(i) * 0.7f +
                        static_cast<float>(range) * 1.1f
                    );
  const float noise = syntheticHash(i, range);
  float v = (0.55f * wave + 0.45f * noise) * level;
  if (v < 0.f) v = 0.f;
  if (v > 1.f) v = 1.f;
  return v;
}

inline std::vector<float> syntheticSeries(
    int count, int range, float level, float phase = 0.f
) {
  std::vector<float> out;
  out.reserve(count > 0 ? static_cast<std::size_t>(count) : 0);
  for (int i = 0; i < count; ++i)
    out.push_back(syntheticValue(i, range, level, phase));
  return out;
}

}  // namespace toolkit
}  // namespace glim
