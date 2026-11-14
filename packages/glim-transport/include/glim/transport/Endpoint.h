#pragma once

#include <glim/transport/Config.h>
#include <glim/transport/Inbox.h>
#include <glim/transport/Outbox.h>
#include <glim/transport/Requests.h>
#include <glim/transport/Transport.h>

#include <coroutine>
#include <cstdint>
#include <functional>
#include <vector>

namespace glim::transport {

class Endpoint;

// Requests started under an owner end with it: they are never resumed and
// the peer is told to cancel.
class Owner {
 public:
  explicit Owner(Endpoint& endpoint);
  ~Owner();
  Owner(const Owner&) = delete;
  Owner& operator=(const Owner&) = delete;

 private:
  friend class Endpoint;
  Endpoint* endpoint_;
};

class Call {
 public:
  bool await_ready() const noexcept { return false; }
  bool await_suspend(std::coroutine_handle<> handle);
  Reply await_resume() { return std::move(result_); }

 private:
  friend class Endpoint;
  Call(Endpoint& endpoint, Owner& owner, Outgoing message, std::uint64_t timeout)
      : endpoint_(endpoint), owner_(owner), message_(std::move(message)), timeout_(timeout) {}

  Endpoint& endpoint_;
  Owner& owner_;
  Outgoing message_;
  std::uint64_t timeout_;
  Reply result_;
};

// One side of a link: streams, priorities, credits and request ids over a
// Transport. Single-threaded; time comes only from tick().
class Endpoint {
 public:
  using Handler = std::function<void(const Message&)>;

  Endpoint(Transport& transport, Config config, Handler handler);
  ~Endpoint();
  Endpoint(const Endpoint&) = delete;
  Endpoint& operator=(const Endpoint&) = delete;

  bool post(
    Lane lane, std::uint16_t channel, std::vector<std::byte> payload, bool supersede = false
  );
  // A timeout of 0 waits until reply, owner drop or reset.
  Call request(
    Owner& owner,
    Lane lane,
    std::uint16_t channel,
    std::vector<std::byte> payload,
    std::uint64_t timeout = 0
  );
  bool reply(const Message& request, std::vector<std::byte> payload);
  bool fail(const Message& request, std::vector<std::byte> payload);

  void poll();
  void flush();
  void tick(std::uint64_t now);
  // The peer restarted: fail all pending requests and start clean.
  void reset();
  bool broken() const { return broken_; }
  bool idle() const { return outbox_.idle(); }

 private:
  friend class Owner;
  friend class Call;
  class Sink;

  bool start(Call& call, std::coroutine_handle<> handle);
  bool answer(const Message& request, Kind kind, std::vector<std::byte> payload);
  void cancel(const std::vector<Requests::Ended>& ended);
  void release(Owner& owner);
  void breakDown();

  Transport& transport_;
  Config config_;
  Handler handler_;
  Inbox inbox_;
  Outbox outbox_;
  Requests requests_;
  std::vector<Owner*> owners_;
  std::vector<std::byte> frame_;
  std::uint64_t now_ = 0;
  bool broken_ = false;
};

}  // namespace glim::transport
