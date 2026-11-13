#include <glim/wire/Record.h>

namespace glim::wire {

std::optional<std::span<const std::byte>> RecordView::tail(std::uint32_t refOffset) const {
  if (!has(refOffset, kTailRefSize)) {
    return std::span<const std::byte>{};
  }
  const auto offset = loadLe<std::uint32_t>(p_ + refOffset);
  const auto length = loadLe<std::uint32_t>(p_ + refOffset + 4);
  if (length == 0) {
    return std::span<const std::byte>{};
  }
  const auto total = size();
  if (offset < fixedSize() || offset > total || length > total - offset) {
    return std::nullopt;
  }
  return std::span<const std::byte>{p_ + offset, length};
}

}  // namespace glim::wire
