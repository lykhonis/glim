#include <glim/transport/Ring.h>

#include <glim/wire/Endian.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstring>

namespace glim::transport {

static_assert(std::endian::native == std::endian::little, "ring indices are shared little-endian");

namespace {

constexpr std::size_t kHead = 0;
constexpr std::size_t kTail = 4;
constexpr std::size_t kCapacity = 8;

std::atomic_ref<std::uint32_t> index(std::span<std::byte> region, std::size_t at) {
  return std::atomic_ref<std::uint32_t>(*reinterpret_cast<std::uint32_t*>(region.data() + at));
}

bool fits(std::span<std::byte> region, std::uint32_t capacity) {
  return validRingCapacity(capacity) && region.size() >= ringBytes(capacity) &&
         reinterpret_cast<std::uintptr_t>(region.data()) % 8 == 0;
}

constexpr std::uint32_t padded(std::size_t length) {
  return static_cast<std::uint32_t>((length + 4 + 7) & ~std::size_t{7});
}

void copyIn(std::span<std::byte> data, std::uint32_t at, std::span<const std::byte> src) {
  if (src.empty()) {
    return;
  }
  const auto pos = at & (data.size() - 1);
  const auto first = std::min(src.size(), data.size() - pos);
  std::memcpy(data.data() + pos, src.data(), first);
  std::memcpy(data.data(), src.data() + first, src.size() - first);
}

void copyOut(std::span<const std::byte> data, std::uint32_t at, std::span<std::byte> dst) {
  if (dst.empty()) {
    return;
  }
  const auto pos = at & (data.size() - 1);
  const auto first = std::min(dst.size(), data.size() - pos);
  std::memcpy(dst.data(), data.data() + pos, first);
  std::memcpy(dst.data() + first, data.data(), dst.size() - first);
}

}  // namespace

bool initRing(std::span<std::byte> region, std::uint32_t capacity) {
  if (!fits(region, capacity)) {
    return false;
  }
  std::memset(region.data(), 0, kRingHeaderSize);
  wire::storeLe(region.data() + kCapacity, capacity);
  return true;
}

Io RingWriter::push(
    std::span<std::byte> region, std::span<const std::byte> head, std::span<const std::byte> body
) {
  const auto length = head.size() + body.size();
  if (!fits(region, capacity_) || length > maxRingEntry(capacity_)) {
    return Io::Corrupt;
  }
  const auto used = head_ - index(region, kTail).load(std::memory_order_acquire);
  if (used > capacity_) {
    return Io::Corrupt;
  }
  const auto need = padded(length);
  if (need > capacity_ - used) {
    return Io::Again;
  }
  const auto data = region.subspan(kRingHeaderSize, capacity_);
  std::byte prefix[4];
  wire::storeLe(prefix, static_cast<std::uint32_t>(length));
  copyIn(data, head_, prefix);
  copyIn(data, head_ + 4, head);
  copyIn(data, head_ + 4 + static_cast<std::uint32_t>(head.size()), body);
  head_ += need;
  index(region, kHead).store(head_, std::memory_order_release);
  return Io::Ok;
}

Io RingReader::pop(std::span<std::byte> region, std::vector<std::byte>& out) {
  if (!fits(region, capacity_)) {
    return Io::Corrupt;
  }
  const auto used = index(region, kHead).load(std::memory_order_acquire) - tail_;
  if (used == 0) {
    return Io::Again;
  }
  if (used > capacity_ || used % 8 != 0) {
    return Io::Corrupt;
  }
  const auto data = region.subspan(kRingHeaderSize, capacity_);
  std::byte prefix[4];
  copyOut(data, tail_, prefix);
  const auto length = wire::loadLe<std::uint32_t>(prefix);
  if (length > maxRingEntry(capacity_) || padded(length) > used) {
    return Io::Corrupt;
  }
  out.resize(length);
  copyOut(data, tail_ + 4, out);
  tail_ += padded(length);
  index(region, kTail).store(tail_, std::memory_order_release);
  return Io::Ok;
}

}  // namespace glim::transport
