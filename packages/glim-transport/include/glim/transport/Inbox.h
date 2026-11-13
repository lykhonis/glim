#pragma once

#include <glim/transport/Config.h>

#include <array>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace glim::transport {

// A complete message; `payload` is valid only during delivery.
struct Message {
  Kind kind = Kind::Event;
  Lane lane = Lane::Control;
  std::uint16_t channel = 0;
  std::uint32_t request = 0;
  std::span<const std::byte> payload;
};

class InboxSink {
 public:
  virtual void deliver(const Message& message) = 0;
  virtual void release(Lane lane, std::uint32_t bytes) = 0;
  virtual bool granted(Lane lane, std::uint32_t bytes) = 0;

 protected:
  ~InboxSink() = default;
};

// Receive side: reassembles chunks under the G8 limits. A stream that breaks
// a rule is dropped and its bytes released; a frame that cannot be trusted
// at all (bad header, credit overrun) makes accept() return false.
class Inbox {
 public:
  explicit Inbox(const Config& config) : config_(config) {}

  bool accept(std::span<const std::byte> frame, InboxSink& sink);
  void reset();

 private:
  struct Partial {
    ChunkHeader header;
    std::vector<std::byte> data;
  };

  void release(InboxSink& sink, Lane lane, std::size_t bytes);
  void drop(InboxSink& sink, std::uint32_t stream);
  void deliver(InboxSink& sink, const ChunkHeader& header, std::span<const std::byte> payload);

  Config config_;
  std::unordered_map<std::uint32_t, Partial> open_;
  std::array<std::uint32_t, kLanes> held_{};
};

}  // namespace glim::transport
