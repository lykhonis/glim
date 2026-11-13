#pragma once

#include <glim/wire/Endian.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace glim::wire {

// Header: u16 type, u8 flags, u8 fixed words, u32 size (K73, K78).
// The fixed part (header + fields) is `words * 8` bytes; the tail follows
// it up to `size`. Readers default fields past the fixed part, so a field
// never aliases tail bytes when an older sender omits it.
inline constexpr std::uint32_t kAlign = 8;
inline constexpr std::uint32_t kHeaderSize = 8;
inline constexpr std::uint32_t kTypeOffset = 0;
inline constexpr std::uint32_t kFlagsOffset = 2;
inline constexpr std::uint32_t kWordsOffset = 3;
inline constexpr std::uint32_t kSizeOffset = 4;
inline constexpr std::uint32_t kTailRefSize = 8;
inline constexpr std::uint32_t kMaxFixedSize = 255 * kAlign;

inline constexpr std::uint8_t kMustUnderstand = 1u << 0;

constexpr std::uint32_t alignUp(std::uint32_t n) { return (n + kAlign - 1) & ~(kAlign - 1); }

class RecordView {
 public:
  explicit RecordView(const std::byte* p) : p_(p) {}

  std::uint16_t type() const { return loadLe<std::uint16_t>(p_ + kTypeOffset); }
  std::uint8_t flags() const { return loadLe<std::uint8_t>(p_ + kFlagsOffset); }
  std::uint32_t fixedSize() const {
    return std::uint32_t{loadLe<std::uint8_t>(p_ + kWordsOffset)} * kAlign;
  }
  std::uint32_t size() const { return loadLe<std::uint32_t>(p_ + kSizeOffset); }

  bool has(std::uint32_t offset, std::uint32_t width) const {
    return offset >= kHeaderSize && width <= fixedSize() && offset <= fixedSize() - width;
  }

  template <Scalar T>
  T field(std::uint32_t offset, T fallback = T{}) const {
    return has(offset, sizeof(T)) ? loadLe<T>(p_ + offset) : fallback;
  }

  // Tail reference at `refOffset`: u32 offset from record start, u32 length.
  // Absent reference reads as empty; a reference outside the tail is nullopt.
  std::optional<std::span<const std::byte>> tail(std::uint32_t refOffset) const;

  std::span<const std::byte> bytes() const { return {p_, size()}; }

 private:
  const std::byte* p_;
};

}  // namespace glim::wire
