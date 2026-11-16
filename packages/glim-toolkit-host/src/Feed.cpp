#include <glim/toolkit/host/Feed.h>

#include <glim/trace.h>

#include <algorithm>
#include <utility>

namespace glim::toolkit::host {

Feed::Feed(Signaled signaled) : signaled_(std::move(signaled)) {}

bool Feed::publish(FeedBuffer buffer) {
  std::lock_guard lock(mutex_);
  if (buffer.imageId == 0 || buffer.fence < lastFence_) return false;
  lastFence_ = buffer.fence;
  pending_.push_back(buffer);
  ++stats_.published;
  return true;
}

std::uint32_t Feed::acquire() {
  const std::uint64_t reached = signaled_ ? signaled_() : 0;
  std::lock_guard lock(mutex_);
  std::uint64_t ready = 0;
  while (!pending_.empty() && pending_.front().fence <= reached) {
    current_ = pending_.front().imageId;
    pending_.pop_front();
    ++ready;
  }
  if (ready > 1) stats_.dropped += ready - 1;
  if (ready == 0 && !pending_.empty()) ++stats_.fenceMisses;
  stats_.lag = static_cast<std::uint32_t>(pending_.size());
  stats_.maxLag = std::max(stats_.maxLag, stats_.lag);
  if (current_ != 0) ++stats_.acquired;
  shown_[shownAt_] = current_;
  shownAt_ = (shownAt_ + 1) % kHeld;
  if (trace::enabled()) {
    trace::counter("feed_lag", stats_.lag);
    trace::counter("feed_fence_misses", static_cast<double>(stats_.fenceMisses));
  }
  return current_;
}

std::optional<std::size_t> Feed::claim(std::span<const std::uint32_t> ring) {
  const std::uint64_t reached = signaled_ ? signaled_() : 0;
  std::lock_guard lock(mutex_);
  const auto shown = [&](std::uint32_t id) {
    return std::ranges::find(shown_, id) != shown_.end();
  };
  for (std::size_t i = 0; i < ring.size(); ++i) {
    if (!shown(ring[i]) &&
        std::ranges::find(pending_, ring[i], &FeedBuffer::imageId) == pending_.end()) {
      return i;
    }
  }
  // Pending fences are in order, so with two ready the front is ready too,
  // and the next acquire would drop it anyway.
  if (pending_.size() >= 2 && pending_[1].fence <= reached) {
    const auto slot = std::ranges::find(ring, pending_.front().imageId);
    if (slot != ring.end()) {
      pending_.pop_front();
      ++stats_.dropped;
      return static_cast<std::size_t>(slot - ring.begin());
    }
  }
  ++stats_.reuseWaits;
  return std::nullopt;
}

FeedStats Feed::stats() const {
  std::lock_guard lock(mutex_);
  return stats_;
}

}  // namespace glim::toolkit::host
