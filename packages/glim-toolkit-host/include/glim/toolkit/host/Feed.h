#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <span>

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
  // Claims that found no buffer to write (producer ahead of the GPU or the frame).
  std::uint64_t reuseWaits = 0;
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
  // Index into `ring` the producer may write next: a buffer neither shown
  // by a frame that may still be on the GPU nor pending, else the oldest
  // pending one when a newer one is ready too (dropped, as acquire would).
  // nullopt means wait: the producer is ahead of its own GPU or of the frame.
  std::optional<std::size_t> claim(std::span<const std::uint32_t> ring);
  FeedStats stats() const;

  // Frame being encoded plus up to three on the GPU (Metal's drawable cap).
  static constexpr std::size_t kHeld = 4;

 private:
  Signaled signaled_;
  mutable std::mutex mutex_;
  std::deque<FeedBuffer> pending_;
  std::uint32_t current_ = 0;
  std::array<std::uint32_t, kHeld> shown_{};
  std::size_t shownAt_ = 0;
  std::uint64_t lastFence_ = 0;
  FeedStats stats_;
};

}  // namespace glim::toolkit::host
