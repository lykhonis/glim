#pragma once

#include <glim/transport/Transport.h>

#include <cstddef>
#include <memory>
#include <utility>

namespace glim::transport {

// In-process link for tests, web workers and hosts without a wasm runtime.
// Each direction holds at most `capacity` bytes of frames.
class DirectTransport final : public Transport {
 public:
  static std::pair<DirectTransport, DirectTransport> pair(std::size_t capacity);

  std::size_t maxFrame() const override;
  Io send(std::span<const std::byte> head, std::span<const std::byte> body) override;
  Io receive(std::vector<std::byte>& frame) override;

  struct Queue;

 private:
  DirectTransport(std::shared_ptr<Queue> tx, std::shared_ptr<Queue> rx)
      : tx_(std::move(tx)), rx_(std::move(rx)) {}

  std::shared_ptr<Queue> tx_;
  std::shared_ptr<Queue> rx_;
};

}  // namespace glim::transport
