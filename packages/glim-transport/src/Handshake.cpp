#include <glim/transport/Handshake.h>

#include <glim/wire/Validate.h>
#include <glim/wire/Writer.h>

#include <algorithm>
#include <array>

namespace glim::transport {

namespace {

constexpr std::uint32_t kProtocolAt = 8;
constexpr std::uint32_t kChunkAt = 12;
constexpr std::uint32_t kWindowAt = 16;
constexpr std::uint32_t kOpenAt = 32;
constexpr std::uint32_t kShareAt = 36;
constexpr std::uint32_t kChannelsAt = 40;
constexpr std::uint32_t kTermsSize = 48;
constexpr std::uint32_t kReasonAt = 8;
constexpr std::uint32_t kRejectSize = 16;
constexpr std::uint32_t kChannelSize = 8;
constexpr std::uint8_t kRequired = 1;
constexpr std::uint32_t kMinChunk = 256;

constexpr std::array kChannelTail{wire::TailRule{kChannelsAt, kChannelSize}};

bool checkReject(wire::RecordView record) {
  const auto reason = record.field<std::uint32_t>(kReasonAt);
  return reason >= 1 && reason <= static_cast<std::uint32_t>(Refusal::Config);
}

constexpr std::array kRules{
  wire::TypeRule{.type = kHelloRecord, .tails = kChannelTail},
  wire::TypeRule{.type = kWelcomeRecord, .tails = kChannelTail},
  wire::TypeRule{.type = kRejectRecord, .tails = {}, .check = checkReject},
};

void writeTerms(std::uint16_t type, const Terms& terms, std::vector<std::byte>& out) {
  auto record = wire::Writer(out).begin(type, kTermsSize);
  record.set(kProtocolAt, terms.protocol).set(kChunkAt, terms.config.chunk);
  for (std::size_t lane = 0; lane < kLanes; ++lane) {
    record.set(static_cast<std::uint32_t>(kWindowAt + lane * 4), terms.config.window[lane]);
  }
  record.set(kOpenAt, terms.config.maxOpenStreams).set(kShareAt, terms.config.bulkShare);
  auto tail =
    record.allocTail(kChannelsAt, static_cast<std::uint32_t>(terms.channels.size() * kChannelSize));
  for (std::size_t i = 0; i < terms.channels.size(); ++i) {
    const auto& offer = terms.channels[i];
    auto* at = tail.data() + i * kChannelSize;
    wire::storeLe(at, offer.channel);
    wire::storeLe(at + 2, offer.version);
    wire::storeLe(at + 4, offer.required ? kRequired : std::uint8_t{0});
  }
}

Terms readTerms(wire::RecordView record) {
  Terms terms;
  terms.protocol = record.field<std::uint32_t>(kProtocolAt);
  terms.config.chunk = record.field<std::uint32_t>(kChunkAt);
  for (std::size_t lane = 0; lane < kLanes; ++lane) {
    terms.config.window[lane] =
      record.field<std::uint32_t>(static_cast<std::uint32_t>(kWindowAt + lane * 4));
  }
  terms.config.maxOpenStreams = record.field<std::uint32_t>(kOpenAt);
  terms.config.bulkShare = record.field<std::uint32_t>(kShareAt);
  const auto tail = *record.tail(kChannelsAt);
  for (std::size_t at = 0; at < tail.size(); at += kChannelSize) {
    terms.channels.push_back(
      {.channel = wire::loadLe<std::uint16_t>(&tail[at]),
       .version = wire::loadLe<std::uint16_t>(&tail[at + 2]),
       .required = (wire::loadLe<std::uint8_t>(&tail[at + 4]) & kRequired) != 0}
    );
  }
  return terms;
}

std::optional<wire::RecordView> single(std::span<const std::byte> message) {
  if (wire::validate(message, kRules) != wire::Status::Ok) {
    return std::nullopt;
  }
  std::optional<wire::RecordView> found;
  bool many = false;
  wire::forEachKnown(message, kRules, [&](wire::RecordView record) {
    many = many || found.has_value();
    found = record;
  });
  return many ? std::nullopt : found;
}

bool usable(const Config& config) {
  return config.chunk >= kMinChunk && config.maxOpenStreams != 0 &&
         std::ranges::all_of(config.window, [](auto window) { return window != 0; });
}

Config narrower(const Config& a, const Config& b) {
  Config out{
    .chunk = std::min(a.chunk, b.chunk),
    .window = {},
    .maxOpenStreams = std::min(a.maxOpenStreams, b.maxOpenStreams),
    .bulkShare = std::min(a.bulkShare, b.bulkShare),
  };
  for (std::size_t lane = 0; lane < kLanes; ++lane) {
    out.window[lane] = std::min(a.window[lane], b.window[lane]);
  }
  return out;
}

}  // namespace

Answer negotiate(const Terms& hello, const Support& support) {
  if (hello.protocol < support.oldestProtocol) {
    return Refusal::Protocol;
  }
  Terms agreed{
    .protocol = std::min(hello.protocol, support.protocol),
    .config = narrower(hello.config, support.limits),
    .channels = {},
  };
  if (!usable(agreed.config)) {
    return Refusal::Config;
  }
  for (const auto& offer : hello.channels) {
    const auto known = std::ranges::find(support.channels, offer.channel, &ChannelSupport::channel);
    if (known != support.channels.end() && offer.version >= known->oldest) {
      agreed.channels.push_back(
        {.channel = offer.channel, .version = std::min(offer.version, known->newest)}
      );
    } else if (offer.required) {
      return Refusal::Channel;
    }
  }
  return agreed;
}

void writeHello(const Terms& hello, std::vector<std::byte>& out) {
  writeTerms(kHelloRecord, hello, out);
}

void writeAnswer(const Answer& answer, std::vector<std::byte>& out) {
  if (const auto* terms = std::get_if<Terms>(&answer)) {
    writeTerms(kWelcomeRecord, *terms, out);
    return;
  }
  wire::Writer(out)
    .begin(kRejectRecord, kRejectSize)
    .set(kReasonAt, static_cast<std::uint32_t>(std::get<Refusal>(answer)));
}

std::optional<Terms> readHello(std::span<const std::byte> message) {
  const auto record = single(message);
  if (!record || record->type() != kHelloRecord) {
    return std::nullopt;
  }
  return readTerms(*record);
}

Answer readAnswer(std::span<const std::byte> message) {
  const auto record = single(message);
  if (record && record->type() == kWelcomeRecord) {
    auto terms = readTerms(*record);
    if (usable(terms.config)) {
      return terms;
    }
  }
  if (record && record->type() == kRejectRecord) {
    return static_cast<Refusal>(record->field<std::uint32_t>(kReasonAt));
  }
  return Refusal::Malformed;
}

}  // namespace glim::transport
