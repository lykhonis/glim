#include <glim/transport/Outbox.h>

#include <algorithm>

namespace glim::transport {

namespace {

Io sendHeader(Transport& transport, const ChunkHeader& header, std::span<const std::byte> body) {
  std::byte head[kChunkHeaderSize];
  encodeChunk(header, head);
  return transport.send(head, body);
}

}  // namespace

Outbox::Outbox(const Config& config) : config_(config), credit_(config.window) {}

bool Outbox::push(Outgoing message) {
  const auto lane = laneIndex(message.lane);
  if (message.payload.size() > config_.window[lane]) {
    return false;
  }
  auto& queue = lanes_[lane];
  if (message.supersede) {
    std::erase_if(queue, [&](const Stream& stream) {
      if (!stream.message.supersede || stream.message.channel != message.channel) {
        return false;
      }
      if (stream.started) {
        aborts_.push_back({stream.id, message.lane});
      }
      return true;
    });
  }
  queue.push_back({nextStream_++, std::move(message)});
  return true;
}

bool Outbox::withdraw(Lane lane, std::uint32_t request) {
  return std::erase_if(lanes_[laneIndex(lane)], [&](const Stream& stream) {
           return !stream.started && stream.message.kind == Kind::Request &&
                  stream.message.request == request;
         }) != 0;
}

bool Outbox::grant(Lane lane, std::uint32_t bytes) {
  auto& credit = credit_[laneIndex(lane)];
  if (bytes > config_.window[laneIndex(lane)] - credit) {
    return false;
  }
  credit += bytes;
  return true;
}

void Outbox::returnCredit(Lane lane, std::uint32_t bytes) { owed_[laneIndex(lane)] += bytes; }

bool Outbox::sendable(std::size_t lane) const {
  if (lanes_[lane].empty()) {
    return false;
  }
  const auto& stream = lanes_[lane].front();
  return credit_[lane] > 0 || stream.sent == stream.message.payload.size();
}

std::optional<std::size_t> Outbox::pick() const {
  const auto bulk = laneIndex(Lane::Bulk);
  if (config_.bulkShare != 0 && turn_ % config_.bulkShare == config_.bulkShare - 1 &&
      sendable(bulk)) {
    return bulk;
  }
  for (std::size_t lane = 0; lane < kLanes; ++lane) {
    if (sendable(lane)) {
      return lane;
    }
  }
  return std::nullopt;
}

Io Outbox::flushControl(Transport& transport) {
  for (std::size_t lane = 0; lane < kLanes; ++lane) {
    if (owed_[lane] == 0) {
      continue;
    }
    const ChunkHeader header{
        .flags = chunk::kCredit, .lane = static_cast<Lane>(lane), .size = owed_[lane]
    };
    if (const auto io = sendHeader(transport, header, {}); io != Io::Ok) {
      return io;
    }
    owed_[lane] = 0;
  }
  while (!aborts_.empty()) {
    const auto abort = aborts_.front();
    const ChunkHeader header{.stream = abort.stream, .flags = chunk::kAbort, .lane = abort.lane};
    if (const auto io = sendHeader(transport, header, {}); io != Io::Ok) {
      return io;
    }
    aborts_.pop_front();
  }
  return Io::Ok;
}

Io Outbox::flush(Transport& transport) {
  if (const auto io = flushControl(transport); io != Io::Ok) {
    return io;
  }
  const auto limit = static_cast<std::uint32_t>(
      std::min<std::size_t>(config_.chunk, transport.maxFrame() - kChunkHeaderSize)
  );
  while (const auto lane = pick()) {
    auto& stream = lanes_[*lane].front();
    const auto& message = stream.message;
    const auto size = static_cast<std::uint32_t>(message.payload.size());
    const auto remaining = size - stream.sent;
    const auto length = std::min({limit, remaining, credit_[*lane]});
    const bool end = length == remaining;
    const ChunkHeader header{
        .stream = stream.id,
        .flags = static_cast<std::uint8_t>(
            (stream.started ? 0 : chunk::kStart) | (end ? chunk::kEnd : 0)
        ),
        .lane = message.lane,
        .channel = message.channel,
        .size = size,
        .kind = message.kind,
        .request = message.request
    };
    const auto body = std::span(message.payload).subspan(stream.sent, length);
    if (const auto io = sendHeader(transport, header, body); io != Io::Ok) {
      return io;
    }
    stream.started = true;
    stream.sent += length;
    credit_[*lane] -= length;
    ++turn_;
    if (end) {
      lanes_[*lane].pop_front();
    }
  }
  return Io::Ok;
}

void Outbox::reset() {
  for (auto& lane : lanes_) {
    lane.clear();
  }
  credit_ = config_.window;
  owed_ = {};
  aborts_.clear();
  turn_ = 0;
}

bool Outbox::idle() const {
  return aborts_.empty() && std::ranges::all_of(owed_, [](auto bytes) { return bytes == 0; }) &&
         std::ranges::all_of(lanes_, [](const auto& lane) { return lane.empty(); });
}

}  // namespace glim::transport
