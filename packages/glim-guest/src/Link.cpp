#include <glim/guest/Link.h>

#include <utility>
#include <vector>

namespace glim::guest {

namespace t = transport;

Link::Link(
  t::Imports& imports,
  std::function<std::span<std::byte>()> memory,
  std::span<std::byte> region,
  std::uint32_t offset,
  t::Terms hello,
  t::Endpoint::Handler handler
)
    : imports_(imports),
      memory_(std::move(memory)),
      region_(region),
      offset_(offset),
      hello_(std::move(hello)),
      handler_(std::move(handler)) {}

std::uint32_t Link::init(std::uint32_t protocol, std::uint32_t capacity) {
  if (state_ != State::Idle || protocol < hello_.protocol) {
    return 0;
  }
  const auto announce = t::writeAnnounce(region_, offset_, hello_.protocol, capacity);
  if (!announce) {
    return 0;
  }
  transport_ = std::make_unique<t::RingTransport>(memory_, announce->tx, announce->rx);
  std::vector<std::byte> message;
  t::writeHello(hello_, message);
  if (transport_->send({}, message) != t::Io::Ok) {
    return 0;
  }
  state_ = State::Offered;
  imports_.send();
  return offset_;
}

void Link::answer() {
  std::vector<std::byte> frame;
  const auto io = transport_->receive(frame);
  if (io == t::Io::Again) {
    return;
  }
  auto answer = io == t::Io::Ok ? t::readAnswer(frame) : t::Answer{t::Refusal::Malformed};
  if (auto* terms = std::get_if<t::Terms>(&answer)) {
    agreed_ = std::move(*terms);
    endpoint_ = std::make_unique<t::Endpoint>(*transport_, agreed_.config, handler_);
    state_ = State::Ready;
    return;
  }
  refusal_ = std::get<t::Refusal>(answer);
  state_ = State::Refused;
  imports_.log(t::kLogWarning, "glim: host refused the handshake");
}

void Link::recv() {
  if (state_ == State::Offered) {
    answer();
  }
  if (state_ == State::Ready) {
    endpoint_->poll();
    flush();
  }
}

void Link::tick(std::uint64_t now) {
  if (state_ == State::Ready) {
    endpoint_->tick(now);
    flush();
  }
}

void Link::flush() {
  if (state_ != State::Ready) {
    return;
  }
  const bool output = !endpoint_->idle();
  endpoint_->flush();
  if (endpoint_->broken()) {
    state_ = State::Broken;
  } else if (output) {
    imports_.send();
  }
}

}  // namespace glim::guest
