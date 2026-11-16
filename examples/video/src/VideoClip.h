#pragma once

#include <cstddef>

namespace glim {
namespace toolkit {

constexpr int kClipWidth = 640;
constexpr int kClipHeight = 360;
constexpr double kClipFps = 30.0;
// One more than host::Feed::kHeld, so the producer never waits at frame
// rate.
constexpr std::size_t kClipRing = 5;

}  // namespace toolkit
}  // namespace glim
