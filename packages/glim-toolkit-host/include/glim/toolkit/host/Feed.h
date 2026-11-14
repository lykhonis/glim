#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>

namespace glim::toolkit::host {

// One buffer a foreign producer finished on its own queue: the image id it
// was wrapped as, and the fence value its queue signals once writes land.
struct FeedBuffer {
  std::uint32_t imageId = 0;
  std::uint64_t fence = 0;
};

struct FeedStats {
  std::uint64_t published = 0;
  std::uint64_t acquired = 0;
  // Frames that wanted a newer buffer whose fence had not passed yet.
  std::uint64_t fenceMisses = 0;
  // Ready buffers never shown because a newer one was ready too.
  std::uint64_t dropped = 0;
  // Published but not yet ready, at the last acquire.
  std::uint32_t lag = 0;
  std::uint32_t maxLag = 0;
};

// Texture-mode frame source (K86). The producer publishes from any thread;
// the frame samples the newest buffer whose fence has passed and never
// waits, so a slow producer costs a stale frame, not a stalled one.
class Feed {
 public:
  using Signaled = std::function<std::uint64_t()>;

  explicit Feed(Signaled signaled);

  // False for id 0 or a fence below the last published one.
  bool publish(FeedBuffer buffer);
  // Image id to sample this frame; 0 until the first buffer is ready.
  std::uint32_t acquire();
  FeedStats stats() const;

 private:
  Signaled signaled_;
  mutable std::mutex mutex_;
  std::deque<FeedBuffer> pending_;
  std::uint32_t current_ = 0;
  std::uint64_t lastFence_ = 0;
  FeedStats stats_;
};

}  // namespace glim::toolkit::host
