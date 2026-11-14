#pragma once

#include <glim/transport/Endpoint.h>
#include <glim/transport/GuestAbi.h>
#include <glim/transport/Handshake.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>

namespace glim::guest {

// The guest half of the doorbell ABI: glim_init, glim_recv and glim_tick
// forward here. Rings live in `region` (linkBytes(capacity) bytes at
// `offset` in guest memory); the endpoint starts once the host welcomes the
// Hello.
class Link {
 public:
  enum class State { Idle, Offered, Ready, Refused, Broken };

  Link(
    transport::Imports& imports,
    std::function<std::span<std::byte>()> memory,
    std::span<std::byte> region,
    std::uint32_t offset,
    transport::Terms hello,
    transport::Endpoint::Handler handler
  );

  std::uint32_t init(std::uint32_t protocol, std::uint32_t capacity);
  void recv();
  void tick(std::uint64_t now);

  State state() const { return state_; }
  std::optional<transport::Refusal> refusal() const { return refusal_; }
  const transport::Terms& agreed() const { return agreed_; }
  // Valid while Ready.
  transport::Endpoint* endpoint() { return endpoint_.get(); }
  // Flushes and rings host_send.
  void flush();

 private:
  void answer();

  transport::Imports& imports_;
  std::function<std::span<std::byte>()> memory_;
  std::span<std::byte> region_;
  std::uint32_t offset_;
  transport::Terms hello_;
  transport::Terms agreed_;
  transport::Endpoint::Handler handler_;
  std::unique_ptr<transport::RingTransport> transport_;
  std::unique_ptr<transport::Endpoint> endpoint_;
  std::optional<transport::Refusal> refusal_;
  State state_ = State::Idle;
};

}  // namespace glim::guest
