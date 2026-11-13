#pragma once

#include <glim/transport/RingTransport.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

// Doorbell ABI (G2, K74). Everything else travels through the two rings in
// guest memory.
//
// Guest exports:
//   u32  glim_init(u32 protocol, u32 ring_capacity)  announce offset, 0 on failure
//   void glim_recv(void)                             host wrote to the guest's rx ring
//   void glim_tick(u64 now)                          frame tick, drives timeouts
// Host imports (module "glim"):
//   void host_send(void)                             guest wrote to its tx ring
//   u64  host_now(void)
//   void host_log(u32 level, u32 ptr, u32 len)

namespace glim::transport {

// Announce block written by glim_init: {u32 magic, u32 protocol,
// u32 tx offset, u32 tx capacity, u32 rx offset, u32 rx capacity}, padded to
// 32. tx is guest to host. Offsets are from the start of guest memory.
inline constexpr std::uint32_t kAnnounceMagic = 0x4D494C47;
inline constexpr std::size_t kAnnounceSize = 32;

struct Announce {
  std::uint32_t protocol = 0;
  RingSpec tx;
  RingSpec rx;
};

// Bytes a guest reserves for the announce block and both rings.
constexpr std::size_t linkBytes(std::uint32_t capacity) {
  return kAnnounceSize + 2 * ringBytes(capacity);
}

// Lays out the block and both rings in `region`, which lies at `offset` in
// guest memory; nullopt if the region is too small or misaligned.
std::optional<Announce> writeAnnounce(
    std::span<std::byte> region,
    std::uint32_t offset,
    std::uint32_t protocol,
    std::uint32_t capacity
);

// Host side: copies the block once and checks both rings lie inside
// `memory`, are 8-aligned, at least kMinRingCapacity, and do not overlap
// each other or the block.
std::optional<Announce> readAnnounce(std::span<const std::byte> memory, std::uint32_t offset);

// The host imports as the guest sees them; a wasm build forwards to the
// real imports, tests call a host directly.
class Imports {
 public:
  virtual void send() = 0;
  virtual std::uint64_t now() = 0;
  virtual void log(std::uint32_t level, std::string_view text) = 0;

 protected:
  ~Imports() = default;
};

}  // namespace glim::transport
