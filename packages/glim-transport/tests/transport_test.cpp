#include <glim/transport/DirectTransport.h>
#include <glim/transport/Endpoint.h>
#include <glim/transport/RingTransport.h>
#include <glim/transport/Task.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <random>
#include <vector>

namespace {

namespace t = glim::transport;
using Bytes = std::vector<std::byte>;

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

Bytes bytes(std::size_t size, std::uint8_t seed) {
  Bytes out(size);
  for (std::size_t i = 0; i < size; ++i) {
    out[i] = static_cast<std::byte>(seed + i * 31);
  }
  return out;
}

struct Received {
  t::Kind kind;
  t::Lane lane;
  std::uint16_t channel;
  std::uint32_t request;
  Bytes payload;
};

struct Side {
  std::vector<Received> got;
  std::function<void(const t::Message&)> onMessage;
  t::Endpoint endpoint;

  Side(t::Transport& transport, t::Config config)
      : endpoint(transport, config, [this](const t::Message& m) {
          got.push_back({m.kind, m.lane, m.channel, m.request, {m.payload.begin(), m.payload.end()}}
          );
          if (onMessage) {
            onMessage(m);
          }
        }) {}
};

struct Link {
  std::pair<t::DirectTransport, t::DirectTransport> wires;
  Side a;
  Side b;

  explicit Link(t::Config config = {}, std::size_t capacity = 1 << 20)
      : wires(t::DirectTransport::pair(capacity)),
        a(wires.first, config),
        b(wires.second, config) {}

  void pump(int rounds = 64) {
    for (int i = 0; i < rounds; ++i) {
      a.endpoint.flush();
      b.endpoint.poll();
      b.endpoint.flush();
      a.endpoint.poll();
    }
  }
};

void chunkHeader() {
  const t::ChunkHeader in{
      .stream = 9,
      .flags = t::chunk::kStart | t::chunk::kEnd,
      .lane = t::Lane::Frame,
      .channel = 0x1234,
      .size = 77,
      .kind = t::Kind::Request,
      .request = 5
  };
  std::byte raw[t::kChunkHeaderSize];
  t::encodeChunk(in, raw);
  const auto out = t::decodeChunk(raw);
  expect(
      out && out->stream == 9 && out->flags == in.flags && out->lane == t::Lane::Frame &&
          out->channel == 0x1234 && out->size == 77 && out->kind == t::Kind::Request &&
          out->request == 5,
      "chunk header round-trip"
  );
  raw[5] = std::byte{4};
  expect(!t::decodeChunk(raw), "unknown lane rejected");
  raw[5] = std::byte{0};
  raw[4] = std::byte{0x10};
  expect(!t::decodeChunk(raw), "unknown flag rejected");
  expect(!t::decodeChunk(std::span(raw).first(10)), "short header rejected");
}

struct RingMemory {
  std::vector<std::uint64_t> words;
  std::span<std::byte> all() { return std::as_writable_bytes(std::span(words)); }
};

void ring() {
  constexpr std::uint32_t cap = 64;
  RingMemory memory{std::vector<std::uint64_t>(t::ringBytes(cap) / 8)};
  expect(t::initRing(memory.all(), cap), "init");
  expect(!t::initRing(memory.all(), 48), "capacity must be a power of two");
  t::RingWriter writer(cap);
  t::RingReader reader(cap);
  Bytes out;
  expect(reader.pop(memory.all(), out) == t::Io::Again, "empty ring");
  for (std::uint8_t round = 0; round < 20; ++round) {
    const auto head = bytes(5, round);
    const auto body = bytes(13 + round % 3, round + 1);
    expect(writer.push(memory.all(), head, body) == t::Io::Ok, "push");
    expect(reader.pop(memory.all(), out) == t::Io::Ok, "pop");
    auto want = head;
    want.insert(want.end(), body.begin(), body.end());
    expect(out == want, "entry survives wrap");
  }
  expect(writer.push(memory.all(), bytes(56, 0), {}) == t::Io::Ok, "fill");
  expect(writer.push(memory.all(), bytes(1, 0), {}) == t::Io::Again, "full ring");
  expect(writer.push(memory.all(), bytes(57, 0), {}) == t::Io::Corrupt, "oversize entry");
  expect(reader.pop(memory.all(), out) == t::Io::Ok && out.size() == 56, "drain");

  auto* index = reinterpret_cast<std::uint32_t*>(memory.all().data());
  index[0] += 4096;
  expect(reader.pop(memory.all(), out) == t::Io::Corrupt, "head past capacity");
  index[0] -= 4096;
  index[1] += 4096;
  expect(writer.push(memory.all(), bytes(1, 0), {}) == t::Io::Corrupt, "tail past head");
  index[1] -= 4096;

  t::RingReader honest(cap);
  RingMemory fresh{std::vector<std::uint64_t>(t::ringBytes(cap) / 8)};
  t::initRing(fresh.all(), cap);
  std::uint32_t bogus = 1000;
  std::memcpy(fresh.all().data() + t::kRingHeaderSize, &bogus, 4);
  reinterpret_cast<std::uint32_t*>(fresh.all().data())[0] = 8;
  expect(honest.pop(fresh.all(), out) == t::Io::Corrupt, "length past head");
  expect(honest.pop(fresh.all().first(20), out) == t::Io::Corrupt, "short region");
}

void ringTransport() {
  constexpr std::uint32_t cap = t::kMinRingCapacity;
  RingMemory memory{std::vector<std::uint64_t>(2 * t::ringBytes(cap) / 8)};
  const auto all = memory.all();
  t::initRing(all.first(t::ringBytes(cap)), cap);
  t::initRing(all.subspan(t::ringBytes(cap)), cap);
  const t::RingSpec first{0, cap};
  const t::RingSpec second{static_cast<std::uint32_t>(t::ringBytes(cap)), cap};
  auto view = [&memory] { return memory.all(); };
  t::RingTransport guest(view, first, second);
  t::RingTransport host(view, second, first);
  Side g(guest, {});
  Side h(host, {});
  const auto big = bytes(200'000, 3);
  g.endpoint.post(t::Lane::Frame, 1, big);
  for (int i = 0; i < 64 && h.got.empty(); ++i) {
    g.endpoint.flush();
    h.endpoint.poll();
    h.endpoint.flush();
    g.endpoint.poll();
  }
  expect(h.got.size() == 1 && h.got[0].payload == big, "large message across rings");
  expect(!g.endpoint.broken() && !h.endpoint.broken(), "rings healthy");
}

void roundTrip() {
  Link link;
  link.a.endpoint.post(t::Lane::Input, 7, bytes(10, 1));
  link.a.endpoint.post(t::Lane::Bulk, 8, bytes(100'000, 2));
  link.a.endpoint.post(t::Lane::Control, 9, {});
  link.pump();
  const auto& got = link.b.got;
  expect(got.size() == 3, "three messages");
  expect(got.size() == 3 && got[0].channel == 9 && got[0].payload.empty(), "empty control first");
  expect(got.size() == 3 && got[1].channel == 7 && got[1].payload == bytes(10, 1), "input next");
  expect(got.size() == 3 && got[2].payload == bytes(100'000, 2), "bulk reassembled");
}

void priorities() {
  t::Config config;
  config.bulkShare = 0;
  Link link(config);
  link.a.endpoint.post(t::Lane::Bulk, 4, bytes(3, 0));
  link.a.endpoint.post(t::Lane::Frame, 3, bytes(3, 0));
  link.a.endpoint.post(t::Lane::Input, 2, bytes(3, 0));
  link.a.endpoint.post(t::Lane::Control, 1, bytes(3, 0));
  link.pump();
  bool ordered = link.b.got.size() == 4;
  for (std::size_t i = 0; ordered && i < 4; ++i) {
    ordered = link.b.got[i].channel == i + 1;
  }
  expect(ordered, "lanes drain in priority order");
}

void interleaving() {
  Link link({}, 128 * 1024);
  link.a.endpoint.post(t::Lane::Frame, 1, bytes(1'000'000, 5));
  link.a.endpoint.flush();
  link.b.endpoint.poll();
  expect(link.b.got.empty(), "big frame still in flight");
  link.a.endpoint.post(t::Lane::Control, 2, bytes(4, 6));
  link.pump();
  expect(
      link.b.got.size() == 2 && link.b.got[0].channel == 2 && link.b.got[1].channel == 1,
      "control passes a frame in flight"
  );
}

void bulkShare() {
  t::Config config;
  config.chunk = 1024;
  config.bulkShare = 4;
  auto [a, b] = t::DirectTransport::pair(1 << 24);
  t::Outbox outbox(config);
  outbox.push({.lane = t::Lane::Frame, .payload = bytes(64 * 1024, 0)});
  outbox.push({.lane = t::Lane::Bulk, .payload = bytes(4 * 1024, 0)});
  outbox.flush(a);
  Bytes frame;
  std::vector<t::Lane> order;
  while (b.receive(frame) == t::Io::Ok) {
    order.push_back(t::decodeChunk(frame)->lane);
  }
  expect(
      order.size() == 68 && order[3] == t::Lane::Bulk && order[7] == t::Lane::Bulk &&
          order[2] == t::Lane::Frame,
      "bulk gets every 4th chunk"
  );
}

void credits() {
  t::Config config;
  config.chunk = 1024;
  config.window[t::laneIndex(t::Lane::Frame)] = 4096;
  Link link(config);
  expect(!link.a.endpoint.post(t::Lane::Frame, 1, bytes(4097, 0)), "message over window refused");
  link.a.endpoint.post(t::Lane::Frame, 1, bytes(4096, 1));
  link.a.endpoint.post(t::Lane::Frame, 2, bytes(4096, 2));
  link.a.endpoint.flush();
  link.a.endpoint.flush();
  Bytes frame;
  int frames = 0;
  std::vector<Bytes> sent;
  while (link.wires.second.receive(frame) == t::Io::Ok) {
    ++frames;
    sent.push_back(frame);
  }
  expect(frames == 4, "sender stops at the credit window");
  Link second(config);
  second.a.endpoint.post(t::Lane::Frame, 1, bytes(4096, 1));
  second.a.endpoint.post(t::Lane::Frame, 2, bytes(4096, 2));
  second.pump();
  expect(
      second.b.got.size() == 2 && second.b.got[1].payload == bytes(4096, 2),
      "credit returns and the second message completes"
  );
}

void creditOverrun() {
  t::Config config;
  config.window[t::laneIndex(t::Lane::Input)] = 64;
  Link link(config);
  t::ChunkHeader header{.stream = 1, .flags = t::chunk::kStart, .lane = t::Lane::Input, .size = 64};
  std::byte head[t::kChunkHeaderSize];
  t::encodeChunk(header, head);
  link.wires.first.send(head, bytes(40, 0));
  header.stream = 2;
  header.flags = t::chunk::kStart;
  t::encodeChunk(header, head);
  link.wires.first.send(head, bytes(40, 0));
  link.b.endpoint.poll();
  expect(link.b.endpoint.broken(), "peer ignoring credit breaks the link");

  Link grant(config);
  header = {.flags = t::chunk::kCredit, .lane = t::Lane::Input, .size = 1};
  t::encodeChunk(header, head);
  grant.wires.first.send(head, {});
  grant.b.endpoint.poll();
  expect(grant.b.endpoint.broken(), "credit grant beyond window breaks the link");
}

void streamViolations() {
  t::Config config;
  config.maxOpenStreams = 1;
  Link link(config);
  auto send = [&](t::ChunkHeader header, std::size_t length) {
    std::byte head[t::kChunkHeaderSize];
    t::encodeChunk(header, head);
    link.wires.first.send(head, bytes(length, 0));
  };
  const auto lane = t::Lane::Bulk;
  send({.stream = 1, .flags = t::chunk::kStart, .lane = lane, .size = 8}, 4);
  send({.stream = 2, .flags = t::chunk::kStart, .lane = lane, .size = 8}, 4);
  send({.stream = 2, .flags = t::chunk::kEnd, .lane = lane, .size = 8}, 4);
  send({.stream = 1, .flags = t::chunk::kEnd, .lane = lane, .size = 8}, 8);
  send({.stream = 3, .flags = t::chunk::kStart | t::chunk::kEnd, .lane = lane, .size = 9}, 4);
  send(
      {.stream = 4, .flags = t::chunk::kStart | t::chunk::kEnd, .lane = lane, .size = 1u << 30}, 1
  );
  send({.stream = 5, .flags = t::chunk::kStart, .lane = lane, .size = 8}, 4);
  send({.stream = 5, .flags = t::chunk::kAbort, .lane = lane}, 0);
  send({.stream = 5, .flags = t::chunk::kEnd, .lane = lane, .size = 8}, 4);
  link.b.endpoint.poll();
  link.b.endpoint.flush();
  expect(link.b.got.empty(), "every broken stream is dropped");
  expect(!link.b.endpoint.broken(), "stream violations do not break the link");
  Bytes frame;
  std::uint32_t returned = 0;
  while (link.wires.first.receive(frame) == t::Io::Ok) {
    const auto header = t::decodeChunk(frame);
    if (header && header->flags == t::chunk::kCredit && header->lane == lane) {
      returned += header->size;
    }
  }
  expect(returned == 4 + 4 + 4 + 8 + 4 + 1 + 4 + 4, "dropped bytes return their credit");
}

void supersede() {
  t::Config config;
  config.chunk = 1024;
  Link link(config);
  link.a.endpoint.post(t::Lane::Frame, 1, bytes(8192, 1), true);
  link.a.endpoint.post(t::Lane::Frame, 1, bytes(8192, 2), true);
  link.pump();
  expect(
      link.b.got.size() == 1 && link.b.got[0].payload == bytes(8192, 2),
      "queued stale frame is dropped"
  );

  Link inFlight(config, 4096);
  inFlight.a.endpoint.post(t::Lane::Frame, 1, bytes(8192, 3), true);
  inFlight.a.endpoint.flush();
  inFlight.b.endpoint.poll();
  inFlight.a.endpoint.post(t::Lane::Frame, 1, bytes(8192, 4), true);
  inFlight.pump();
  expect(
      inFlight.b.got.size() == 1 && inFlight.b.got[0].payload == bytes(8192, 4),
      "stale frame in flight is aborted"
  );
}

t::Task ask(
    t::Endpoint& endpoint, t::Owner& owner, std::optional<t::Reply>& out, std::uint64_t timeout = 0
) {
  out = co_await endpoint.request(owner, t::Lane::Input, 3, bytes(4, 9), timeout);
}

void requests() {
  Link link;
  link.b.onMessage = [&](const t::Message& m) {
    if (m.kind == t::Kind::Request && m.payload.size() == 4) {
      link.b.endpoint.reply(m, bytes(2, 1));
    }
  };
  t::Owner owner(link.a.endpoint);
  std::optional<t::Reply> reply;
  ask(link.a.endpoint, owner, reply);
  expect(!reply, "request suspends");
  link.pump();
  expect(reply && reply->outcome == t::Outcome::Ok && reply->payload == bytes(2, 1), "reply");

  link.b.onMessage = [&](const t::Message& m) {
    if (m.kind == t::Kind::Request) {
      link.b.endpoint.fail(m, bytes(1, 7));
    }
  };
  reply.reset();
  ask(link.a.endpoint, owner, reply);
  link.pump();
  expect(reply && reply->outcome == t::Outcome::Error && reply->payload == bytes(1, 7), "error");

  link.b.onMessage = {};
  reply.reset();
  link.a.endpoint.tick(100);
  ask(link.a.endpoint, owner, reply, 50);
  link.a.endpoint.tick(120);
  link.pump();
  expect(!link.b.got.empty() && link.b.got.back().kind == t::Kind::Request, "request delivered");
  link.a.endpoint.tick(149);
  expect(!reply, "not yet timed out");
  link.a.endpoint.tick(150);
  expect(reply && reply->outcome == t::Outcome::Timeout, "timeout");
  link.pump();
  expect(link.b.got.back().kind == t::Kind::Cancel, "timeout cancels on the peer");

  const auto seen = link.b.got.size();
  reply.reset();
  ask(link.a.endpoint, owner, reply, 10);
  link.a.endpoint.tick(200);
  link.pump();
  expect(
      reply && reply->outcome == t::Outcome::Timeout && link.b.got.size() == seen,
      "unsent request is withdrawn, not cancelled"
  );

  reply.reset();
  ask(link.a.endpoint, owner, reply);
  link.a.endpoint.reset();
  expect(reply && reply->outcome == t::Outcome::Reset, "reset fails pending");
}

void ownerDrop() {
  Link link;
  std::optional<t::Reply> reply;
  {
    t::Owner owner(link.a.endpoint);
    ask(link.a.endpoint, owner, reply);
    link.pump();
  }
  link.pump();
  expect(!reply, "dropped owner is never resumed");
  const auto& got = link.b.got;
  expect(
      got.size() == 2 && got[0].kind == t::Kind::Request && got[1].kind == t::Kind::Cancel &&
          got[1].request == got[0].request,
      "peer gets the cancel"
  );
  link.b.endpoint.reply(
      {.kind = t::Kind::Request, .lane = t::Lane::Input, .request = got[0].request, .payload = {}},
      {}
  );
  link.pump();
  expect(!reply, "late reply is ignored");

  auto* endpoint = new t::Endpoint(link.wires.first, {}, [](const t::Message&) {});
  t::Owner orphan(*endpoint);
  ask(*endpoint, orphan, reply);
  delete endpoint;
  expect(!reply, "endpoint teardown destroys pending without resuming");
  ask(link.a.endpoint, orphan, reply);
  expect(reply && reply->outcome == t::Outcome::Reset, "detached owner fails at once");
}

void corruptLink() {
  Link link;
  std::byte junk[8]{};
  link.wires.first.send(junk, {});
  t::Owner owner(link.b.endpoint);
  std::optional<t::Reply> reply;
  ask(link.b.endpoint, owner, reply);
  link.b.endpoint.poll();
  expect(link.b.endpoint.broken(), "bad header breaks the link");
  expect(reply && reply->outcome == t::Outcome::Reset, "break fails pending");
  expect(!link.b.endpoint.post(t::Lane::Control, 1, {}), "broken link refuses posts");
  link.b.endpoint.reset();
  expect(!link.b.endpoint.broken() && link.b.endpoint.post(t::Lane::Control, 1, {}), "reset heals");
}

void randomTraffic() {
  t::Config config;
  config.chunk = 512;
  config.window = {4096, 8192, 1 << 16, 1 << 16};
  Link link(config, 8192);
  std::mt19937 rng(1234);
  std::vector<Received> expectedAtB, expectedAtA;
  for (int i = 0; i < 400; ++i) {
    const auto lane = static_cast<t::Lane>(rng() % t::kLanes);
    const auto limit = config.window[t::laneIndex(lane)];
    const auto size = rng() % 3 == 0 ? rng() % limit : rng() % 64;
    const auto channel = static_cast<std::uint16_t>(i);
    auto payload = bytes(size, static_cast<std::uint8_t>(i));
    auto& side = rng() % 2 == 0 ? link.a : link.b;
    auto& expected = &side == &link.a ? expectedAtB : expectedAtA;
    expected.push_back({t::Kind::Event, lane, channel, 0, payload});
    side.endpoint.post(lane, channel, std::move(payload));
    if (rng() % 4 == 0) {
      link.pump(1);
    }
  }
  link.pump(4096);
  auto matches = [](const std::vector<Received>& want, const std::vector<Received>& got) {
    if (want.size() != got.size()) {
      return false;
    }
    for (std::size_t lane = 0; lane < t::kLanes; ++lane) {
      std::vector<const Received*> a, b;
      for (const auto& m : want) {
        if (t::laneIndex(m.lane) == lane) a.push_back(&m);
      }
      for (const auto& m : got) {
        if (t::laneIndex(m.lane) == lane) b.push_back(&m);
      }
      if (a.size() != b.size()) {
        return false;
      }
      for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i]->channel != b[i]->channel || a[i]->payload != b[i]->payload) {
          return false;
        }
      }
    }
    return true;
  };
  expect(matches(expectedAtB, link.b.got), "random traffic a to b intact, in lane order");
  expect(matches(expectedAtA, link.a.got), "random traffic b to a intact, in lane order");
  expect(!link.a.endpoint.broken() && !link.b.endpoint.broken(), "random traffic healthy");
}

}  // namespace

int main() {
  chunkHeader();
  ring();
  ringTransport();
  roundTrip();
  priorities();
  interleaving();
  bulkShare();
  credits();
  creditOverrun();
  streamViolations();
  supersede();
  requests();
  ownerDrop();
  corruptLink();
  randomTraffic();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "glim-transport tests passed\n";
  return 0;
}
