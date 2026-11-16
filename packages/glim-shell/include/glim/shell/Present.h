#pragma once

#include <functional>
#include <string>

#include <glim/math.h>

namespace glim::shell {

enum class PresentKind { Share };

enum class PresentStatus { Done, Cancelled, Unsupported, Busy };

struct PresentRequest {
  PresentKind kind = PresentKind::Share;
  std::string text;
  // Window-logical rect the sheet points at (popovers, iPad).
  Rect anchor{};
};

struct PresentResult {
  PresentStatus status = PresentStatus::Unsupported;
  // Share: the chosen service or activity id.
  std::string detail;
};

// Runs once on the main thread, after the system UI is gone.
using PresentDone = std::function<void(const PresentResult&)>;

}  // namespace glim::shell
