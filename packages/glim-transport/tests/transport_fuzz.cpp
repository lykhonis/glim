#include <glim/transport/Endpoint.h>
#include <glim/transport/Ring.h>
#include <glim/transport/Task.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

namespace {

namespace t = glim::transport;

// Hands the fuzz input to an endpoint as frames: {u16 length, bytes}.
class Feed final : public t::Transport {
 public:
  explicit Feed(std::span<const std::byte> input) : input_(input) {}

  std::size_t maxFrame() const override { return 1 << 16; }
  t::Io send(std::span<const std::byte>, std::span<const std::byte>) override {
    return ++sent_ % 3 == 0 ? t::Io::Again : t::Io::Ok;
  }
  t::Io receive(std::vector<std::byte>& frame) override {
    if (input_.size() < 2) {
      return t::Io::Again;
    }
    std::uint16_t length;
    std::memcpy(&length, input_.data(), 2);
    input_ = input_.subspan(2);
    length = static_cast<std::uint16_t>(std::min<std::size_t>(length, input_.size()));
    frame.assign(input_.begin(), input_.begin() + length);
    input_ = input_.subspan(length);
    return t::Io::Ok;
  }

 private:
  std::span<const std::byte> input_;
  int sent_ = 0;
};

t::Task ask(t::Endpoint& endpoint, t::Owner& owner) {
  co_await endpoint.request(owner, t::Lane::Input, 1, {}, 5);
}

void ring(std::span<const std::byte> input) {
  constexpr std::uint32_t cap = 256;
  std::vector<std::uint64_t> words(t::ringBytes(cap) / 8);
  auto region = std::as_writable_bytes(std::span(words));
  std::memcpy(region.data(), input.data(), std::min(input.size(), region.size()));
  t::RingReader reader(cap);
  t::RingWriter writer(cap);
  std::vector<std::byte> out;
  for (int i = 0; i < 64 && reader.pop(region, out) == t::Io::Ok; ++i) {
  }
  writer.push(region, input.first(std::min<std::size_t>(input.size(), 16)), {});
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto input = std::as_bytes(std::span(data, size));
  ring(input);

  t::Config config;
  config.window = {256, 1024, 4096, 4096};
  config.maxOpenStreams = 4;
  config.chunk = 128;
  Feed feed(input);
  t::Endpoint* self = nullptr;
  t::Endpoint endpoint(feed, config, [&self](const t::Message& message) {
    if (message.kind == t::Kind::Request) {
      self->reply(message, {message.payload.begin(), message.payload.end()});
    }
  });
  self = &endpoint;
  t::Owner owner(endpoint);
  for (int i = 0; i < 4; ++i) {
    ask(endpoint, owner);
  }
  endpoint.poll();
  endpoint.tick(10);
  endpoint.flush();
  endpoint.flush();
  return 0;
}
