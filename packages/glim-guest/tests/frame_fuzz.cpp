#include <glim/guest/FrameRecords.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  std::vector<std::byte> message(size);
  if (size != 0) {
    std::memcpy(message.data(), data, size);
  }
  if (const auto frame = glim::guest::readFrame(message, glim::paint::ImageStore{})) {
    std::vector<std::byte> again;
    glim::guest::writeFrame(*frame, again);
    if (!glim::guest::readFrame(again, glim::paint::ImageStore{})) {
      __builtin_trap();
    }
  }
  return 0;
}
