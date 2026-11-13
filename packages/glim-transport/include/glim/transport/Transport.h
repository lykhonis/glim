#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace glim::transport {

enum class Io { Ok, Again, Corrupt };

// Moves whole frames in order. A frame is sent as header plus body so callers
// never join them first.
class Transport {
 public:
  virtual ~Transport() = default;
  virtual std::size_t maxFrame() const = 0;
  virtual Io send(std::span<const std::byte> head, std::span<const std::byte> body) = 0;
  virtual Io receive(std::vector<std::byte>& frame) = 0;
};

}  // namespace glim::transport
