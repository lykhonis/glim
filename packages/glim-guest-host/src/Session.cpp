#include <glim/guest_host/Session.h>

#include <utility>
#include <vector>

namespace glim::guest_host {

namespace t = transport;

namespace {

constexpr int kMaxRounds = 16;
constexpr std::size_t kMaxLog = 1024;

}  // namespace

Session::Session(Runtime& runtime, Options options, t::Endpoint::Handler handler)
    : runtime_(runtime), options_(std::move(options)), handler_(std::move(handler)) {}

Session::~Session() = default;

std::uint64_t Session::now() { return options_.clock ? options_.clock() : 0; }

void Session::log(std::uint32_t level, std::string_view text) {
  if (options_.logger) {
    options_.logger(level, text.substr(0, kMaxLog));
  }
}

bool Session::start() {
  if (state_ != State::Idle) {
    return false;
  }
  const auto offset = runtime_.init(options_.support.protocol, options_.ringCapacity);
  const auto announce = t::readAnnounce(runtime_.memory(), offset);
  if (!announce) {
    refusal_ = t::Refusal::Malformed;
    state_ = State::Refused;
    return false;
  }
  transport_ = std::make_unique<t::RingTransport>(
    [this] { return runtime_.memory(); }, announce->rx, announce->tx
  );
  state_ = State::Offered;
  service();
  return state_ == State::Offered || state_ == State::Ready;
}

void Session::handshake() {
  std::vector<std::byte> frame;
  const auto io = transport_->receive(frame);
  if (io == t::Io::Again) {
    return;
  }
  const auto hello = io == t::Io::Ok ? t::readHello(frame) : std::nullopt;
  const auto answer =
    hello ? t::negotiate(*hello, options_.support) : t::Answer{t::Refusal::Malformed};
  std::vector<std::byte> reply;
  t::writeAnswer(answer, reply);
  if (transport_->send({}, reply) != t::Io::Ok) {
    state_ = State::Broken;
    return;
  }
  if (const auto* terms = std::get_if<t::Terms>(&answer)) {
    agreed_ = *terms;
    endpoint_ = std::make_unique<t::Endpoint>(*transport_, agreed_.config, handler_);
    state_ = State::Ready;
  } else {
    refusal_ = std::get<t::Refusal>(answer);
    state_ = State::Refused;
  }
  runtime_.recv();
}

void Session::exchange() {
  endpoint_->poll();
  const bool output = !endpoint_->idle();
  endpoint_->flush();
  if (endpoint_->broken()) {
    state_ = State::Broken;
  } else if (output) {
    runtime_.recv();
  }
}

void Session::service() {
  for (int round = 0; round < kMaxRounds; ++round) {
    pending_ = false;
    if (state_ == State::Offered) {
      handshake();
    } else if (state_ == State::Ready) {
      exchange();
    }
    if (!pending_) {
      return;
    }
  }
}

void Session::tick(std::uint64_t now) {
  if (state_ != State::Ready) {
    return;
  }
  endpoint_->tick(now);
  runtime_.tick(now);
  service();
}

}  // namespace glim::guest_host
