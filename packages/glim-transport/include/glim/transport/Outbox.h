#pragma once

#include <glim/transport/Config.h>
#include <glim/transport/Transport.h>

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace glim::transport {

struct Outgoing {
  Lane lane = Lane::Control;
  Kind kind = Kind::Event;
  std::uint16_t channel = 0;
  std::uint32_t request = 0;
  // A newer message on the same lane and channel replaces this one, aborting
  // it mid-send if needed (stale frames).
  bool supersede = false;
  std::vector<std::byte> payload;
};

// Send side: chunks messages, picks lanes by priority, spends credit, and
// carries credit grants and aborts back to the peer.
class Outbox {
 public:
  explicit Outbox(const Config& config);

  bool push(Outgoing message);
  // Removes a request that has not started sending; false once it has.
  bool withdraw(Lane lane, std::uint32_t request);
  bool grant(Lane lane, std::uint32_t bytes);
  void returnCredit(Lane lane, std::uint32_t bytes);
  Io flush(Transport& transport);
  void reset();
  bool idle() const;

 private:
  struct Stream {
    std::uint32_t id;
    Outgoing message;
    std::uint32_t sent = 0;
    bool started = false;
  };
  struct Abort {
    std::uint32_t stream;
    Lane lane;
  };

  Io flushControl(Transport& transport);
  bool sendable(std::size_t lane) const;
  std::optional<std::size_t> pick() const;

  Config config_;
  std::array<std::deque<Stream>, kLanes> lanes_;
  std::array<std::uint32_t, kLanes> credit_;
  std::array<std::uint32_t, kLanes> owed_{};
  std::deque<Abort> aborts_;
  std::uint32_t nextStream_ = 1;
  std::uint32_t turn_ = 0;
};

}  // namespace glim::transport
