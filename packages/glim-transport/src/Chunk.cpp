#include <glim/transport/Chunk.h>

#include <glim/wire/Endian.h>

#include <algorithm>

namespace glim::transport {

namespace {

constexpr std::uint8_t kKnownFlags = chunk::kStart | chunk::kEnd | chunk::kAbort | chunk::kCredit;
constexpr std::uint8_t kLastKind = static_cast<std::uint8_t>(Kind::Cancel);

}  // namespace

void encodeChunk(const ChunkHeader& header, std::span<std::byte, kChunkHeaderSize> out) {
  std::ranges::fill(out, std::byte{0});
  wire::storeLe(&out[0], header.stream);
  wire::storeLe(&out[4], header.flags);
  wire::storeLe(&out[5], static_cast<std::uint8_t>(header.lane));
  wire::storeLe(&out[6], header.channel);
  wire::storeLe(&out[8], header.size);
  wire::storeLe(&out[12], static_cast<std::uint8_t>(header.kind));
  wire::storeLe(&out[16], header.request);
}

std::optional<ChunkHeader> decodeChunk(std::span<const std::byte> frame) {
  if (frame.size() < kChunkHeaderSize) {
    return std::nullopt;
  }
  const auto flags = wire::loadLe<std::uint8_t>(&frame[4]);
  const auto lane = wire::loadLe<std::uint8_t>(&frame[5]);
  const auto kind = wire::loadLe<std::uint8_t>(&frame[12]);
  if ((flags & ~kKnownFlags) != 0 || lane >= kLanes || kind > kLastKind) {
    return std::nullopt;
  }
  return ChunkHeader{
    .stream = wire::loadLe<std::uint32_t>(&frame[0]),
    .flags = flags,
    .lane = static_cast<Lane>(lane),
    .channel = wire::loadLe<std::uint16_t>(&frame[6]),
    .size = wire::loadLe<std::uint32_t>(&frame[8]),
    .kind = static_cast<Kind>(kind),
    .request = wire::loadLe<std::uint32_t>(&frame[16])
  };
}

}  // namespace glim::transport
