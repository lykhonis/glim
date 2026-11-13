#include <glim/transport/GuestAbi.h>
#include <glim/transport/Handshake.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace t = glim::transport;

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto input = std::as_bytes(std::span(data, size));
  std::vector<std::uint64_t> words((size + 7) / 8);
  std::ranges::copy(input, std::as_writable_bytes(std::span(words)).begin());
  const auto aligned = std::as_bytes(std::span(words)).first(size);

  if (size >= 4) {
    std::uint32_t offset;
    std::memcpy(&offset, data, 4);
    t::readAnnounce(input, offset % (size + 64));
  }

  t::Support support;
  support.channels = {{.channel = 1, .oldest = 1, .newest = 2}};
  if (const auto hello = t::readHello(aligned)) {
    const auto answer = t::negotiate(*hello, support);
    std::vector<std::byte> out;
    t::writeAnswer(answer, out);
    const auto back = t::readAnswer(out);
    if (back.index() != answer.index()) {
      __builtin_trap();
    }
  }
  t::readAnswer(aligned);
  return 0;
}
