#pragma once

#include <glim/transport/Endpoint.h>
#include <glim/transport/GuestAbi.h>
#include <glim/transport/Handshake.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace glim::guest_host {

// A loaded guest as the host drives it (K77): its memory and its exports.
// WAMR or any other engine implements this; it routes the guest's imports
// to the Session.
class Runtime {
 public:
  virtual ~Runtime() = default;
  virtual std::span<std::byte> memory() = 0;
  virtual std::uint32_t init(std::uint32_t protocol, std::uint32_t capacity) = 0;
  virtual void recv() = 0;
  virtual void tick(std::uint64_t now) = 0;
};

// Host end of one guest: checks the ring announce, answers the Hello, then
// runs an Endpoint. Doorbells from the guest only mark work; service() does
// it, so the guest is never re-entered from inside its own call.
class Session final : public transport::Imports {
 public:
  enum class State { Idle, Offered, Ready, Refused, Broken };

  struct Options {
    transport::Support support;
    std::uint32_t ringCapacity = 256 * 1024;
    std::function<std::uint64_t()> clock;
    std::function<void(std::uint32_t, std::string_view)> logger;
  };

  Session(Runtime& runtime, Options options, transport::Endpoint::Handler handler);
  ~Session();

  bool start();
  void service();
  void tick(std::uint64_t now);

  State state() const { return state_; }
  std::optional<transport::Refusal> refusal() const { return refusal_; }
  const transport::Terms& agreed() const { return agreed_; }
  // Valid while Ready.
  transport::Endpoint* endpoint() { return endpoint_.get(); }

  void send() override { pending_ = true; }
  std::uint64_t now() override;
  void log(std::uint32_t level, std::string_view text) override;

 private:
  void handshake();
  void exchange();

  Runtime& runtime_;
  Options options_;
  transport::Endpoint::Handler handler_;
  std::unique_ptr<transport::RingTransport> transport_;
  std::unique_ptr<transport::Endpoint> endpoint_;
  transport::Terms agreed_;
  std::optional<transport::Refusal> refusal_;
  State state_ = State::Idle;
  bool pending_ = false;
};

}  // namespace glim::guest_host
