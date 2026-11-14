#include <glim/transport/GuestAbi.h>

#include <glim/wire/Endian.h>

#include <cstring>

namespace glim::transport {

namespace {

constexpr std::size_t kMagicAt = 0;
constexpr std::size_t kProtocolAt = 4;
constexpr std::size_t kTxAt = 8;
constexpr std::size_t kRxAt = 16;

bool inside(RingSpec ring, std::size_t memory) {
  return validRingCapacity(ring.capacity) && ring.capacity >= kMinRingCapacity &&
         ring.offset % 8 == 0 && ring.offset <= memory &&
         memory - ring.offset >= ringBytes(ring.capacity);
}

bool apart(std::uint64_t a, std::uint64_t aSize, std::uint64_t b, std::uint64_t bSize) {
  return a + aSize <= b || b + bSize <= a;
}

}  // namespace

std::optional<Announce> writeAnnounce(
  std::span<std::byte> region, std::uint32_t offset, std::uint32_t protocol, std::uint32_t capacity
) {
  if (!validRingCapacity(capacity) || capacity < kMinRingCapacity ||
      region.size() < linkBytes(capacity) || offset % 8 != 0) {
    return std::nullopt;
  }
  const auto ring = static_cast<std::uint32_t>(ringBytes(capacity));
  const Announce announce{
    .protocol = protocol,
    .tx = {.offset = offset + static_cast<std::uint32_t>(kAnnounceSize), .capacity = capacity},
    .rx =
      {.offset = offset + static_cast<std::uint32_t>(kAnnounceSize) + ring, .capacity = capacity},
  };
  if (!initRing(region.subspan(kAnnounceSize, ring), capacity) ||
      !initRing(region.subspan(kAnnounceSize + ring, ring), capacity)) {
    return std::nullopt;
  }
  std::memset(region.data(), 0, kAnnounceSize);
  wire::storeLe(&region[kMagicAt], kAnnounceMagic);
  wire::storeLe(&region[kProtocolAt], protocol);
  wire::storeLe(&region[kTxAt], announce.tx.offset);
  wire::storeLe(&region[kTxAt + 4], announce.tx.capacity);
  wire::storeLe(&region[kRxAt], announce.rx.offset);
  wire::storeLe(&region[kRxAt + 4], announce.rx.capacity);
  return announce;
}

std::optional<Announce> readAnnounce(std::span<const std::byte> memory, std::uint32_t offset) {
  if (offset == 0 || offset > memory.size() || memory.size() - offset < kAnnounceSize) {
    return std::nullopt;
  }
  std::byte block[kAnnounceSize];
  std::memcpy(block, memory.data() + offset, kAnnounceSize);
  const Announce announce{
    .protocol = wire::loadLe<std::uint32_t>(&block[kProtocolAt]),
    .tx =
      {.offset = wire::loadLe<std::uint32_t>(&block[kTxAt]),
       .capacity = wire::loadLe<std::uint32_t>(&block[kTxAt + 4])},
    .rx =
      {.offset = wire::loadLe<std::uint32_t>(&block[kRxAt]),
       .capacity = wire::loadLe<std::uint32_t>(&block[kRxAt + 4])},
  };
  const auto& tx = announce.tx;
  const auto& rx = announce.rx;
  if (wire::loadLe<std::uint32_t>(&block[kMagicAt]) != kAnnounceMagic ||
      !inside(tx, memory.size()) || !inside(rx, memory.size()) ||
      !apart(tx.offset, ringBytes(tx.capacity), rx.offset, ringBytes(rx.capacity)) ||
      !apart(offset, kAnnounceSize, tx.offset, ringBytes(tx.capacity)) ||
      !apart(offset, kAnnounceSize, rx.offset, ringBytes(rx.capacity))) {
    return std::nullopt;
  }
  return announce;
}

}  // namespace glim::transport
