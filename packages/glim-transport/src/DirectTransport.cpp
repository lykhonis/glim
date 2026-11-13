#include <glim/transport/DirectTransport.h>

#include <deque>

namespace glim::transport {

struct DirectTransport::Queue {
  std::size_t capacity;
  std::size_t bytes = 0;
  std::deque<std::vector<std::byte>> frames;
};

std::pair<DirectTransport, DirectTransport> DirectTransport::pair(std::size_t capacity) {
  auto ab = std::make_shared<Queue>(Queue{.capacity = capacity, .frames = {}});
  auto ba = std::make_shared<Queue>(Queue{.capacity = capacity, .frames = {}});
  return {DirectTransport(ab, ba), DirectTransport(ba, ab)};
}

std::size_t DirectTransport::maxFrame() const { return tx_->capacity; }

Io DirectTransport::send(std::span<const std::byte> head, std::span<const std::byte> body) {
  const auto size = head.size() + body.size();
  if (size > tx_->capacity) {
    return Io::Corrupt;
  }
  if (size > tx_->capacity - tx_->bytes) {
    return Io::Again;
  }
  auto& frame = tx_->frames.emplace_back(head.begin(), head.end());
  frame.insert(frame.end(), body.begin(), body.end());
  tx_->bytes += size;
  return Io::Ok;
}

Io DirectTransport::receive(std::vector<std::byte>& frame) {
  if (rx_->frames.empty()) {
    return Io::Again;
  }
  frame = std::move(rx_->frames.front());
  rx_->frames.pop_front();
  rx_->bytes -= frame.size();
  return Io::Ok;
}

}  // namespace glim::transport
