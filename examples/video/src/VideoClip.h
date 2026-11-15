#pragma once

#include <cstddef>

namespace glim {
namespace toolkit {

constexpr int kClipWidth = 640;
constexpr int kClipHeight = 360;
constexpr double kClipFps = 30.0;
// Three buffers: one shown, one in flight, one being written.
constexpr std::size_t kClipRing = 3;

}  // namespace toolkit
}  // namespace glim
