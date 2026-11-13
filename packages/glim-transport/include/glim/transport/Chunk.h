#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace glim::transport {

// Send order: control first, bulk last (K75).
enum class Lane : std::uint8_t { Control, Input, Frame, Bulk };
inline constexpr std::size_t kLanes = 4;

constexpr std::size_t laneIndex(Lane lane) { return static_cast<std::size_t>(lane); }

enum class Kind : std::uint8_t { Event, Request, Reply, Error, Cancel };

namespace chunk {
inline constexpr std::uint8_t kStart = 1;
inline constexpr std::uint8_t kEnd = 2;
inline constexpr std::uint8_t kAbort = 4;
inline constexpr std::uint8_t kCredit = 8;
}  // namespace chunk

// Every frame starts with this header. `size` is the declared message size,
// or the granted bytes on a credit frame.
struct ChunkHeader {
  std::uint32_t stream = 0;
  std::uint8_t flags = 0;
  Lane lane = Lane::Control;
  std::uint16_t channel = 0;
  std::uint32_t size = 0;
  Kind kind = Kind::Event;
  std::uint32_t request = 0;
};

inline constexpr std::size_t kChunkHeaderSize = 24;

void encodeChunk(const ChunkHeader& header, std::span<std::byte, kChunkHeaderSize> out);
std::optional<ChunkHeader> decodeChunk(std::span<const std::byte> frame);

}  // namespace glim::transport
