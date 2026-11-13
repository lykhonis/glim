#pragma once

#include <glim/transport/Chunk.h>

#include <array>
#include <cstdint>

namespace glim::transport {

// Both sides must agree; the handshake carries these values.
struct Config {
  std::uint32_t chunk = 32 * 1024;
  // Per-lane credit; also the largest message a lane accepts.
  std::array<std::uint32_t, kLanes> window{64 * 1024, 256 * 1024, 16u << 20, 16u << 20};
  std::uint32_t maxOpenStreams = 64;
  // Every n-th chunk goes to bulk when it is ready, so it is never starved; 0 disables.
  std::uint32_t bulkShare = 8;
};

}  // namespace glim::transport
