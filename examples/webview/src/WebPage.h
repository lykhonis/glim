#pragma once

#include <cstdint>
#include <memory>

#include "glim/paint/Context.h"
#include "glim/toolkit.h"
#include "glim/toolkit/host/SeatView.h"

namespace glim {
namespace toolkit {

inline constexpr std::uint32_t kWebSeat = 1;

// One page per platform, in the mode K86 picks there: Texture renders
// it into buffers Glim samples, Slot hands the host a native view.
class WebPage {
 public:
  virtual ~WebPage() = default;
  virtual void attach(paint::Context&) {}
  // Texture: image id to sample this frame, 0 until the page has drawn.
  virtual std::uint32_t acquire() { return 0; }
  virtual bool input(const LayerInput&) { return false; }
  // Slot: the factory the host calls for kWebSeat; null in Texture
  // mode.
  virtual host::SeatViewFactory seatViews() const { return nullptr; }
  virtual const char* mode() const = 0;
};

std::unique_ptr<WebPage> makeWebPage();

}  // namespace toolkit
}  // namespace glim
