#pragma once

#include <glim/transport/Transport.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace glim::transport {

// Shared ring: {u32 head, u32 tail, u32 capacity, u32 reserved} then
// `capacity` data bytes. Entries are {u32 length, bytes} padded to 8. Each
// side keeps its own index locally and checks the peer's before use; the
// region is passed on every call because guest memory can move when it grows.
inline constexpr std::size_t kRingHeaderSize = 16;
inline constexpr std::uint32_t kMinRingCapacity = 64 * 1024;

constexpr bool validRingCapacity(std::uint32_t capacity) {
  return capacity >= 64 && capacity <= (1u << 30) && (capacity & (capacity - 1)) == 0;
}
constexpr std::size_t ringBytes(std::uint32_t capacity) { return kRingHeaderSize + capacity; }
constexpr std::size_t maxRingEntry(std::uint32_t capacity) { return capacity - 8; }

bool initRing(std::span<std::byte> region, std::uint32_t capacity);

class RingWriter {
 public:
  explicit RingWriter(std::uint32_t capacity) : capacity_(capacity) {}
  Io push(
      std::span<std::byte> region, std::span<const std::byte> head, std::span<const std::byte> body
  );

 private:
  std::uint32_t capacity_;
  std::uint32_t head_ = 0;
};

class RingReader {
 public:
  explicit RingReader(std::uint32_t capacity) : capacity_(capacity) {}
  // Copies the entry out once; callers validate the copy, never the ring.
  Io pop(std::span<std::byte> region, std::vector<std::byte>& out);

 private:
  std::uint32_t capacity_;
  std::uint32_t tail_ = 0;
};

}  // namespace glim::transport
