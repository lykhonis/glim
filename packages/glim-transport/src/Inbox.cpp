#include <glim/transport/Inbox.h>

#include <utility>

namespace glim::transport {

void Inbox::release(InboxSink& sink, Lane lane, std::size_t bytes) {
  if (bytes == 0) {
    return;
  }
  held_[laneIndex(lane)] -= static_cast<std::uint32_t>(bytes);
  sink.release(lane, static_cast<std::uint32_t>(bytes));
}

void Inbox::drop(InboxSink& sink, std::uint32_t stream) {
  const auto it = open_.find(stream);
  if (it == open_.end()) {
    return;
  }
  const auto lane = it->second.header.lane;
  const auto bytes = it->second.data.size();
  open_.erase(it);
  release(sink, lane, bytes);
}

void Inbox::deliver(
  InboxSink& sink, const ChunkHeader& header, std::span<const std::byte> payload
) {
  sink.deliver(
    {.kind = header.kind,
     .lane = header.lane,
     .channel = header.channel,
     .request = header.request,
     .payload = payload}
  );
  release(sink, header.lane, payload.size());
}

bool Inbox::accept(std::span<const std::byte> frame, InboxSink& sink) {
  const auto header = decodeChunk(frame);
  if (!header) {
    return false;
  }
  const auto body = frame.subspan(kChunkHeaderSize);
  if ((header->flags & chunk::kCredit) != 0) {
    return header->flags == chunk::kCredit && body.empty() &&
           sink.granted(header->lane, header->size);
  }
  if ((header->flags & chunk::kAbort) != 0) {
    if (header->flags != chunk::kAbort || !body.empty()) {
      return false;
    }
    const auto it = open_.find(header->stream);
    if (it != open_.end() && it->second.header.lane == header->lane) {
      drop(sink, header->stream);
    }
    return true;
  }

  const auto lane = laneIndex(header->lane);
  if (body.size() > config_.window[lane] - held_[lane]) {
    return false;
  }
  held_[lane] += static_cast<std::uint32_t>(body.size());

  const bool end = (header->flags & chunk::kEnd) != 0;
  if ((header->flags & chunk::kStart) != 0) {
    drop(sink, header->stream);
    if (header->size > config_.window[lane] || body.size() > header->size) {
      release(sink, header->lane, body.size());
    } else if (end) {
      if (body.size() == header->size) {
        deliver(sink, *header, body);
      } else {
        release(sink, header->lane, body.size());
      }
    } else if (open_.size() >= config_.maxOpenStreams) {
      release(sink, header->lane, body.size());
    } else {
      auto& partial = open_[header->stream];
      partial.header = *header;
      partial.data.reserve(header->size);
      partial.data.assign(body.begin(), body.end());
    }
    return true;
  }

  const auto it = open_.find(header->stream);
  if (it == open_.end() || it->second.header.lane != header->lane) {
    release(sink, header->lane, body.size());
    return true;
  }
  auto& partial = it->second;
  if (body.size() > partial.header.size - partial.data.size()) {
    release(sink, header->lane, body.size());
    drop(sink, header->stream);
    return true;
  }
  partial.data.insert(partial.data.end(), body.begin(), body.end());
  if (!end) {
    return true;
  }
  if (partial.data.size() != partial.header.size) {
    drop(sink, header->stream);
    return true;
  }
  const auto done = std::move(partial);
  open_.erase(it);
  deliver(sink, done.header, done.data);
  return true;
}

void Inbox::reset() {
  open_.clear();
  held_ = {};
}

}  // namespace glim::transport
