#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace glim::wire {

template <class T>
concept Scalar = std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::int8_t> ||
                 std::is_same_v<T, std::uint16_t> || std::is_same_v<T, std::int16_t> ||
                 std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::int32_t> ||
                 std::is_same_v<T, std::uint64_t> || std::is_same_v<T, std::int64_t> ||
                 std::is_same_v<T, float> || std::is_same_v<T, double>;

namespace detail {

template <class T>
struct Raw {
  std::byte b[sizeof(T)];

  void toLittle() {
    if constexpr (std::endian::native == std::endian::big) {
      for (std::size_t i = 0; i < sizeof(T) / 2; ++i) {
        std::swap(b[i], b[sizeof(T) - 1 - i]);
      }
    }
  }
};

}  // namespace detail

template <Scalar T>
T loadLe(const std::byte* p) {
  detail::Raw<T> raw;
  std::memcpy(raw.b, p, sizeof(T));
  raw.toLittle();
  return std::bit_cast<T>(raw);
}

template <Scalar T>
void storeLe(std::byte* p, T value) {
  auto raw = std::bit_cast<detail::Raw<T>>(value);
  raw.toLittle();
  std::memcpy(p, raw.b, sizeof(T));
}

}  // namespace glim::wire
