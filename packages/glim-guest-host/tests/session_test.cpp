#include <glim/guest/FrameRecords.h>
#include <glim/guest/Link.h>
#include <glim/guest_host/Session.h>
#include <glim/transport/Task.h>
#include <glim/wire/Writer.h>

#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace t = glim::transport;
namespace g = glim::guest;
namespace h = glim::guest_host;
namespace p = glim::paint;

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

constexpr std::uint16_t kFrameChannel = 1;
constexpr std::uint16_t kInputChannel = 2;
constexpr std::uint32_t kCapacity = t::kMinRingCapacity;
constexpr std::uint32_t kRegionAt = 64;

// Stands in for a wasm engine: guest memory is `arena`, exports call Link,
// imports go to whatever Imports the host attached.
class FakeRuntime final : public h::Runtime, public t::Imports {
 public:
  FakeRuntime(t::Terms hello, t::Endpoint::Handler handler)
      : arena_(t::linkBytes(kCapacity) / 8 + kRegionAt / 8),
        link_(
          *this,
          [this] { return memory(); },
          memory().subspan(kRegionAt, t::linkBytes(kCapacity)),
          kRegionAt,
          std::move(hello),
          std::move(handler)
        ) {}

  void attach(t::Imports& host) { host_ = &host; }
  g::Link& link() { return link_; }

  std::span<std::byte> memory() override { return std::as_writable_bytes(std::span(arena_)); }
  std::uint32_t init(std::uint32_t protocol, std::uint32_t capacity) override {
    return link_.init(protocol, capacity);
  }
  void recv() override { link_.recv(); }
  void tick(std::uint64_t now) override { link_.tick(now); }

  void send() override { host_->send(); }
  std::uint64_t now() override { return host_->now(); }
  void log(std::uint32_t level, std::string_view text) override { host_->log(level, text); }

 private:
  std::vector<std::uint64_t> arena_;
  t::Imports* host_ = nullptr;
  g::Link link_;
};

t::Terms guestHello() {
  t::Terms hello;
  hello.channels = {
    {.channel = kFrameChannel, .version = 3, .required = true},
    {.channel = kInputChannel, .version = 1, .required = false},
    {.channel = 99, .version = 1, .required = false},
  };
  return hello;
}

t::Support hostSupport() {
  t::Support support;
  support.limits.chunk = 16 * 1024;
  support.channels = {
    {.channel = kFrameChannel, .oldest = 1, .newest = 2},
    {.channel = kInputChannel, .oldest = 1, .newest = 1},
  };
  return support;
}

struct Pair {
  std::vector<std::vector<std::byte>> hostGot;
  std::vector<std::string> logs;
  FakeRuntime runtime;
  h::Session session;

  Pair(t::Terms hello, t::Support support, t::Endpoint::Handler guestHandler = {})
      : runtime(
          std::move(hello), guestHandler ? std::move(guestHandler) : [](const t::Message&) {}
        ),
        session(
          runtime,
          {.support = std::move(support),
           .ringCapacity = kCapacity,
           .clock = [] { return std::uint64_t{42}; },
           .logger = [this](std::uint32_t, std::string_view text) { logs.emplace_back(text); }},
          [this](const t::Message& m) { hostGot.emplace_back(m.payload.begin(), m.payload.end()); }
        ) {
    runtime.attach(session);
  }
};

void handshakeAgrees() {
  Pair pair(guestHello(), hostSupport());
  expect(pair.session.start(), "session starts");
  expect(pair.session.state() == h::Session::State::Ready, "host ready");
  expect(pair.runtime.link().state() == g::Link::State::Ready, "guest ready");
  const auto& agreed = pair.runtime.link().agreed();
  expect(agreed.config.chunk == 16 * 1024, "chunk narrowed to the host limit");
  expect(
    agreed.channels.size() == 2 && agreed.channels[0].version == 2 &&
      agreed.channels[1].channel == kInputChannel,
    "frame channel downgraded, unknown optional channel dropped"
  );
  expect(pair.session.agreed().channels.size() == 2, "both sides agree");
  expect(pair.runtime.now() == 42, "host_now reaches the host clock");
}

p::FramePacket smallFrame() {
  p::FramePacket packet;
  packet.logicalSize = {10, 10};
  packet.quads.push_back(p::Quad{0, 0, 10, 10, 1, 0, 0, 1, 0});
  return packet;
}

void framesFlow() {
  Pair pair(guestHello(), hostSupport());
  pair.session.start();
  std::vector<std::byte> frame;
  g::writeFrame(smallFrame(), frame);
  auto* guest = pair.runtime.link().endpoint();
  expect(guest && guest->post(t::Lane::Frame, kFrameChannel, frame, true), "guest posts a frame");
  pair.runtime.link().flush();
  pair.session.service();
  expect(pair.hostGot.size() == 1, "host receives the frame");
  const auto decoded =
    pair.hostGot.empty() ? std::nullopt : g::readFrame(pair.hostGot[0], p::ImageStore{});
  std::vector<std::byte> again;
  if (decoded) {
    g::writeFrame(*decoded, again);
  }
  expect(decoded && again == frame, "frame record survives the rings");
}

t::Task ask(t::Endpoint& endpoint, t::Owner& owner, std::optional<t::Reply>& out) {
  out = co_await endpoint.request(owner, t::Lane::Input, kInputChannel, {std::byte{7}});
}

void requestsCross() {
  g::Link* link = nullptr;
  Pair pair(guestHello(), hostSupport(), [&link](const t::Message& m) {
    if (m.kind == t::Kind::Request) {
      link->endpoint()->reply(m, {m.payload.begin(), m.payload.end()});
    }
  });
  link = &pair.runtime.link();
  pair.session.start();
  t::Owner owner(*pair.session.endpoint());
  std::optional<t::Reply> reply;
  ask(*pair.session.endpoint(), owner, reply);
  pair.session.service();
  expect(
    reply && reply->outcome == t::Outcome::Ok && reply->payload == std::vector{std::byte{7}},
    "host request answered by the guest"
  );
}

void versionSkew() {
  auto newer = hostSupport();
  newer.protocol = t::kProtocolVersion + 2;
  newer.oldestProtocol = t::kProtocolVersion;
  Pair skew(guestHello(), newer);
  skew.session.start();
  expect(
    skew.session.state() == h::Session::State::Ready &&
      skew.session.agreed().protocol == t::kProtocolVersion,
    "newer host speaks the older guest protocol"
  );

  auto strict = hostSupport();
  strict.protocol = t::kProtocolVersion + 2;
  strict.oldestProtocol = t::kProtocolVersion + 1;
  Pair old(guestHello(), strict);
  expect(!old.session.start() || old.session.state() == h::Session::State::Refused, "too old");
  expect(old.session.refusal() == t::Refusal::Protocol, "host refuses an old guest");
  expect(
    old.runtime.link().state() == g::Link::State::Refused &&
      old.runtime.link().refusal() == t::Refusal::Protocol && !old.logs.empty(),
    "guest learns why and logs it"
  );
}

void rejections() {
  auto hello = guestHello();
  hello.channels.push_back({.channel = 77, .version = 1, .required = true});
  Pair missing(hello, hostSupport());
  missing.session.start();
  expect(missing.session.refusal() == t::Refusal::Channel, "required channel unsupported");

  auto tooNew = hostSupport();
  tooNew.channels[0].oldest = 4;
  Pair stale(guestHello(), tooNew);
  stale.session.start();
  expect(stale.session.refusal() == t::Refusal::Channel, "required channel too old");

  auto tiny = hostSupport();
  tiny.limits.chunk = 0;
  Pair config(guestHello(), tiny);
  config.session.start();
  expect(config.session.refusal() == t::Refusal::Config, "unusable config refused");
}

void unknownRecords() {
  std::vector<std::byte> message;
  t::writeHello(guestHello(), message);
  const auto plain = message.size();
  glim::wire::Writer(message).begin(0x0777, 16);
  expect(t::readHello(message).has_value(), "unknown optional record skipped");
  message.resize(plain);
  glim::wire::Writer(message).begin(0x0777, 16, glim::wire::kMustUnderstand);
  expect(!t::readHello(message), "unknown must-understand record rejected");
  message.resize(plain);
  t::writeHello(guestHello(), message);
  expect(!t::readHello(message), "two hellos rejected");
  expect(
    std::get<t::Refusal>(t::readAnswer({})) == t::Refusal::Malformed, "empty answer malformed"
  );
}

void announces() {
  std::vector<std::uint64_t> words(t::linkBytes(kCapacity) / 8 + 8);
  const auto memory = std::as_writable_bytes(std::span(words));
  const auto good = t::writeAnnounce(memory.subspan(64), 64, 1, kCapacity);
  expect(good && t::readAnnounce(memory, 64).has_value(), "announce round-trips");
  expect(!t::readAnnounce(memory, 0), "offset 0 is failure");
  expect(!t::readAnnounce(memory.first(memory.size() - 8), 64), "ring past memory end");
  expect(!t::writeAnnounce(memory.subspan(64), 64, 1, 1024), "ring under minimum");

  auto patch = [&](std::size_t at, std::uint32_t value) {
    auto copy = std::vector<std::byte>(memory.begin(), memory.end());
    std::memcpy(copy.data() + 64 + at, &value, 4);
    return t::readAnnounce(copy, 64).has_value();
  };
  expect(!patch(0, 0), "bad magic");
  expect(!patch(8, good->rx.offset), "rings overlap");
  expect(!patch(8, 64), "ring overlaps the block");
  expect(!patch(8, good->tx.offset + 4), "misaligned ring");
  expect(!patch(12, kCapacity + 1), "capacity not a power of two");
}

class Silent final : public h::Runtime {
 public:
  std::span<std::byte> memory() override { return {}; }
  std::uint32_t init(std::uint32_t, std::uint32_t) override { return 0; }
  void recv() override {}
  void tick(std::uint64_t) override {}
};

void failedInit() {
  Silent runtime;
  h::Session session(runtime, {}, [](const t::Message&) {});
  expect(!session.start(), "guest init failure");
  expect(session.refusal() == t::Refusal::Malformed, "reported as malformed");
}

}  // namespace

int main() {
  handshakeAgrees();
  framesFlow();
  requestsCross();
  versionSkew();
  rejections();
  unknownRecords();
  announces();
  failedInit();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "glim-guest-host tests passed\n";
  return 0;
}
