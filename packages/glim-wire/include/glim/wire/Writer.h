#pragma once

#include <glim/wire/Record.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace glim::wire {

// Appends records to `out` in place. Offsets, not pointers, survive growth.
class Writer {
 public:
  explicit Writer(std::vector<std::byte>& out) : out_(out) {}

  class Builder {
   public:
    template <Scalar T>
    Builder& set(std::uint32_t offset, T value) {
      storeLe(at(offset, sizeof(T)), value);
      return *this;
    }

    // The fixed part after the header, for writing fields in place; valid
    // until the next append to the buffer.
    std::span<std::byte> fields() {
      return {out_.data() + start_ + kHeaderSize, fixedSize_ - kHeaderSize};
    }

    // Reserves a zeroed tail of `length` bytes and returns it for writing in
    // place; the span is valid until the next append to the buffer.
    std::span<std::byte> allocTail(std::uint32_t refOffset, std::uint32_t length);
    Builder& tail(std::uint32_t refOffset, std::span<const std::byte> data);
    Builder& tail(std::uint32_t refOffset, std::string_view text);

   private:
    friend class Writer;
    Builder(std::vector<std::byte>& out, std::size_t start, std::uint32_t fixedSize)
        : out_(out), start_(start), fixedSize_(fixedSize) {}

    std::byte* at(std::uint32_t offset, std::uint32_t width);

    std::vector<std::byte>& out_;
    std::size_t start_;
    std::uint32_t fixedSize_;
  };

  // `fixedSize` covers header and fields; it is rounded up to 8.
  Builder begin(std::uint16_t type, std::uint32_t fixedSize, std::uint8_t flags = 0);

 private:
  std::vector<std::byte>& out_;
};

}  // namespace glim::wire
