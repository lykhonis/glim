#pragma once

#include <glim/gpu/Device.h>
#include <glim/paint/Context.h>
#include <glim/toolkit/host/Feed.h>

#include <array>
#include <cstdint>

#include "VideoClip.h"

// The video example's clip as a browser producer: WebGL2 renders it on
// its own canvas, the canvas becomes a VideoFrame, and the frame is
// copied into a ring of textures on Glim's device that the frame
// samples through Feed.
class WebClip {
 public:
  WebClip();
  ~WebClip();
  WebClip(const WebClip&) = delete;
  WebClip& operator=(const WebClip&) = delete;

  // False when the browser lacks WebGL2 or VideoFrame.
  bool attach(glim::gpu::Device& device, glim::paint::Context& ctx);
  void produce(double timeSeconds);
  std::uint32_t acquire() { return feed_.acquire(); }
  glim::toolkit::host::FeedStats stats() const { return feed_.stats(); }

 private:
  void* device_ = nullptr;
  std::array<void*, glim::toolkit::kClipRing> textures_{};
  std::array<std::uint32_t, glim::toolkit::kClipRing> ids_{};
  std::uint64_t fence_ = 0;
  glim::toolkit::host::Feed feed_;
};
