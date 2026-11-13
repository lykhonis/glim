#pragma once

#include <glim/paint/Scene.h>

#include <cstdint>

// The scene both the wasm guest and the in-process golden record (G13).
// No images or text: those need the resource channel (G4).
namespace glim::smoke {

inline constexpr int kWidth = 96;
inline constexpr int kHeight = 72;
inline constexpr std::uint16_t kFrameChannel = 1;

paint::Scene scene();

}  // namespace glim::smoke
