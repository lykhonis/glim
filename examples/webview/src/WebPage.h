#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "glim/paint/Context.h"
#include "glim/toolkit.h"
#include "glim/toolkit/host/SeatView.h"

namespace glim {
namespace toolkit {

inline constexpr std::uint32_t kWebSeat = 1;
// The bundled page: offline, so CI never needs the network.
inline constexpr const char* kLocalUrl = "about:glim";
inline constexpr const char* kSiteUrl = "https://example.com/";

// What the Glim chrome shows about the page.
struct WebStatus {
  std::string url = kLocalUrl;
  std::string title;
  float progress = 0;
  bool loading = true;
  bool operator==(const WebStatus&) const = default;
};

// One page per platform, in the mode K86 picks there: Texture renders
// it into buffers Glim samples, Slot hands the host a native view.
class WebPage {
 public:
  virtual ~WebPage() = default;
  virtual void attach(paint::Context&) {}
  // Takes what the page drew and its status; true when either changed
  // and the frame should rebuild.
  virtual bool advance() { return false; }
  virtual WebStatus status() const { return {}; }
  // kLocalUrl loads the bundled page.
  virtual void load(const std::string& url) = 0;
  virtual void reload() = 0;
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
