#include <glim/transport/RingTransport.h>

#include <utility>

namespace glim::transport {

RingTransport::RingTransport(std::function<std::span<std::byte>()> memory, RingSpec tx, RingSpec rx)
    : memory_(std::move(memory)), tx_(tx), rx_(rx), writer_(tx.capacity), reader_(rx.capacity) {}

std::span<std::byte> RingTransport::region(RingSpec spec) const {
  const auto memory = memory_();
  if (!validRingCapacity(spec.capacity) || spec.offset > memory.size() ||
      memory.size() - spec.offset < ringBytes(spec.capacity)) {
    return {};
  }
  return memory.subspan(spec.offset, ringBytes(spec.capacity));
}

std::size_t RingTransport::maxFrame() const { return maxRingEntry(tx_.capacity); }

Io RingTransport::send(std::span<const std::byte> head, std::span<const std::byte> body) {
  return writer_.push(region(tx_), head, body);
}

Io RingTransport::receive(std::vector<std::byte>& frame) { return reader_.pop(region(rx_), frame); }

}  // namespace glim::transport
