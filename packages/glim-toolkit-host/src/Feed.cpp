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
  if (trace::enabled()) {
    trace::counter("feed_lag", stats_.lag);
    trace::counter("feed_fence_misses", static_cast<double>(stats_.fenceMisses));
  }
  return current_;
}

FeedStats Feed::stats() const {
  std::lock_guard lock(mutex_);
  return stats_;
}

}  // namespace glim::toolkit::host
