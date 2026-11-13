#pragma once

#include <glim/paint/FramePacket.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace glim::guest {

// Frame channel record types (K73, G1). A frame is one Frame record, then
// draw arrays for the root, then isolates as Begin ... End brackets whose
// arrays and pills apply to the innermost open isolate.
inline constexpr std::uint16_t kFrameRecord = 0x0100;
inline constexpr std::uint16_t kQuadsRecord = 0x0101;
inline constexpr std::uint16_t kBlitsRecord = 0x0102;
inline constexpr std::uint16_t kGradientsRecord = 0x0103;
inline constexpr std::uint16_t kIsolateBeginRecord = 0x0104;
inline constexpr std::uint16_t kIsolateEndRecord = 0x0105;
inline constexpr std::uint16_t kBackdropPillsRecord = 0x0106;
inline constexpr std::uint16_t kLensPillsRecord = 0x0107;

inline constexpr int kMaxIsolateDepth = 64;

// Images are not inline: they travel once on the resource channel, and
// blits reference them by id.
void writeFrame(const paint::FramePacket& packet, std::vector<std::byte>& out);

// Validates framing, then decodes in one pass that reads each source byte
// once and checks values on the copy; nullopt on any error.
std::optional<paint::FramePacket> readFrame(
    std::span<const std::byte> message, paint::ImageStore images
);

// Same, decoding into `out` and reusing its array capacity across frames.
// On failure `out` holds no draws.
bool readFrame(
    std::span<const std::byte> message, paint::ImageStore images, paint::FramePacket& out
);

}  // namespace glim::guest
