#include <glim/wire/Writer.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace glim::wire {

Writer::Builder Writer::begin(std::uint16_t type, std::uint32_t fixedSize, std::uint8_t flags) {
  const auto fixed = alignUp(std::max(fixedSize, kHeaderSize));
  if (fixed > kMaxFixedSize) {
    throw std::length_error("glim::wire: fixed part exceeds 2040 bytes");
  }
  const auto start = out_.size();
  out_.resize(start + fixed);
  std::byte* p = out_.data() + start;
  storeLe(p + kTypeOffset, type);
  storeLe(p + kFlagsOffset, flags);
  storeLe(p + kWordsOffset, static_cast<std::uint8_t>(fixed / kAlign));
  storeLe(p + kSizeOffset, fixed);
  return Builder(out_, start, fixed);
}

std::byte* Writer::Builder::at(std::uint32_t offset, std::uint32_t width) {
  if (offset < kHeaderSize || width > fixedSize_ || offset > fixedSize_ - width) {
    throw std::out_of_range("glim::wire: field outside the fixed part");
  }
  return out_.data() + start_ + offset;
}

std::span<std::byte> Writer::Builder::allocTail(std::uint32_t refOffset, std::uint32_t length) {
  const auto offset = static_cast<std::uint32_t>(out_.size() - start_);
  if (length > UINT32_MAX - kAlign - offset) {
    throw std::length_error("glim::wire: record exceeds u32 size");
  }
  set(refOffset, length == 0 ? 0u : offset).set(refOffset + 4, length);
  out_.resize(start_ + alignUp(offset + length));
  storeLe(out_.data() + start_ + kSizeOffset, static_cast<std::uint32_t>(out_.size() - start_));
  return {out_.data() + start_ + offset, length};
}

Writer::Builder& Writer::Builder::tail(std::uint32_t refOffset, std::span<const std::byte> data) {
  if (data.size() > UINT32_MAX) {
    throw std::length_error("glim::wire: record exceeds u32 size");
  }
  const auto dest = allocTail(refOffset, static_cast<std::uint32_t>(data.size()));
  if (!data.empty()) {
    std::memcpy(dest.data(), data.data(), data.size());
  }
  return *this;
}

Writer::Builder& Writer::Builder::tail(std::uint32_t refOffset, std::string_view text) {
  return tail(refOffset, std::as_bytes(std::span(text.data(), text.size())));
}

}  // namespace glim::wire
