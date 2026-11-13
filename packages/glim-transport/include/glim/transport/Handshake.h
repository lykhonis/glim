#pragma once

#include <glim/transport/Config.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace glim::transport {

// Capability handshake (G3, K80): the guest's first ring entry is a Hello,
// the host answers with one Welcome or Reject entry, then both sides start
// an Endpoint with the agreed Config.
inline constexpr std::uint16_t kHelloRecord = 0x0001;
inline constexpr std::uint16_t kWelcomeRecord = 0x0002;
inline constexpr std::uint16_t kRejectRecord = 0x0003;

inline constexpr std::uint32_t kProtocolVersion = 1;

struct ChannelOffer {
  std::uint16_t channel = 0;
  std::uint16_t version = 0;
  bool required = false;
};

struct Terms {
  std::uint32_t protocol = kProtocolVersion;
  Config config;
  std::vector<ChannelOffer> channels;
};

struct ChannelSupport {
  std::uint16_t channel = 0;
  std::uint16_t oldest = 0;
  std::uint16_t newest = 0;
};

struct Support {
  std::uint32_t protocol = kProtocolVersion;
  std::uint32_t oldestProtocol = kProtocolVersion;
  Config limits;
  std::vector<ChannelSupport> channels;
};

enum class Refusal : std::uint32_t { Malformed = 1, Protocol, Channel, Config };

using Answer = std::variant<Terms, Refusal>;

Answer negotiate(const Terms& hello, const Support& support);

void writeHello(const Terms& hello, std::vector<std::byte>& out);
void writeAnswer(const Answer& answer, std::vector<std::byte>& out);
std::optional<Terms> readHello(std::span<const std::byte> message);
Answer readAnswer(std::span<const std::byte> message);

}  // namespace glim::transport
