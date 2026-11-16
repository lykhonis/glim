#pragma once

#include <string>
#include <utility>

#include "glim/toolkit.h"
#include "glim/toolkit/host/Present.h"

namespace glim {
namespace toolkit {

inline std::string shareText(const shell::PresentResult& r) {
  switch (r.status) {
    case shell::PresentStatus::Done:
      return "Shared via " + r.detail;
    case shell::PresentStatus::Cancelled:
      return "Share cancelled";
    case shell::PresentStatus::Busy:
      return "A sheet is already open";
    case shell::PresentStatus::Unsupported:
      break;
  }
  return "Sharing is not available here";
}

// The share button's action: present the system sheet, then show its
// result.
inline std::function<void()> shareAction(
  const Engine& e, State<std::string> shared
) {
  return [e, shared]() mutable {
    host::present(
      e,
      {.kind = shell::PresentKind::Share, .text = "Glim Video"},
      [shared](const shell::PresentResult& r) mutable {
        shared = shareText(r);
      }
    );
  };
}

}  // namespace toolkit
}  // namespace glim
