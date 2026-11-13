#include <glim/wire/Validate.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

namespace w = glim::wire;

constexpr w::TailRule kTails[] = {{8, 1}, {16, 4}};

bool finite(w::RecordView r) { return std::isfinite(r.field<float>(24)); }

constexpr w::TypeRule kRules[] = {{1, kTails, finite}, {2, kTails}};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  std::vector<std::byte> message(size);
  if (size != 0) {
    std::memcpy(message.data(), data, size);
  }
  if (w::validate(message, kRules) != w::Status::Ok) {
    return 0;
  }
  volatile std::uint32_t sink = 0;
  w::forEachKnown(message, kRules, [&](w::RecordView r) {
    for (const w::TailRule& t : kTails) {
      const auto tail = r.tail(t.refOffset);
      for (std::byte b : *tail) {
        sink = sink + static_cast<std::uint32_t>(b);
      }
    }
    sink = sink + r.field<std::uint32_t>(32);
  });
  return 0;
}
