#pragma once

#include <cstdint>
#include <memory>

#include "glim/paint/Context.h"
#include "glim/toolkit/host/Feed.h"

namespace glim {
namespace toolkit {

class VideoSource {
 public:
  virtual ~VideoSource() = default;
  virtual void attach(paint::Context& ctx) = 0;
  virtual void setPlaying(bool playing) = 0;
  // Image id to show this frame; 0 until the first frame is ready.
  virtual std::uint32_t acquire() = 0;
  virtual host::FeedStats stats() const = 0;
};

std::unique_ptr<VideoSource> makeVideoSource();

}  // namespace toolkit
}  // namespace glim
