#pragma once

#include <glim/transport/Ring.h>

#include <cstdint>
#include <functional>
#include <span>

namespace glim::transport {

struct RingSpec {
  std::uint32_t offset = 0;
  std::uint32_t capacity = 0;
};

// Two rings in one memory, one per direction. `memory` is asked for the
// current base on every call.
class RingTransport final : public Transport {
 public:
  RingTransport(std::function<std::span<std::byte>()> memory, RingSpec tx, RingSpec rx);

  std::size_t maxFrame() const override;
  Io send(std::span<const std::byte> head, std::span<const std::byte> body) override;
  Io receive(std::vector<std::byte>& frame) override;

 private:
  std::span<std::byte> region(RingSpec spec) const;

  std::function<std::span<std::byte>()> memory_;
  RingSpec tx_;
  RingSpec rx_;
  RingWriter writer_;
  RingReader reader_;
};

}  // namespace glim::transport
