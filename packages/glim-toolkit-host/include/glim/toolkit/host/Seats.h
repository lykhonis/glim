#pragma once

#include <glim/paint/Scene.h>
#include <glim/shell/Slot.h>
#include <glim/shell/Window.h>
#include <glim/toolkit/host/SeatView.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace glim {
namespace toolkit {
namespace host {

// Attaches a view per new slot id, repositions on move only, detaches removed.
inline void syncSeats(
    glim::shell::Window& window,
    const glim::paint::Scene& scene,
    std::unordered_map<std::uint32_t, glim::Rect>& attached,
    SeatViewFactory makeView = &makeDefaultSeatView
) {
  std::vector<glim::paint::SlotPlacement> placed;
  glim::paint::collectSlots(scene, &placed);
  std::unordered_map<std::uint32_t, glim::Rect> next;
  for (const auto& p : placed) next[p.id] = p.windowLogical;
  for (auto it = attached.begin(); it != attached.end();) {
    if (next.find(it->first) == next.end()) {
      window.detachSlot(it->first);
      it = attached.erase(it);
    } else {
      ++it;
    }
  }
  for (const auto& p : placed) {
    auto it = attached.find(p.id);
    if (it == attached.end()) {
      if (makeView != nullptr) {
        if (void* view = makeView(SeatRequest{p.id, p.windowLogical})) {
          window.attachSlot(p.id, glim::shell::SlotNative{view});
        }
      }
      attached[p.id] = p.windowLogical;
      window.positionSlot(p.id, p.windowLogical);
    } else if (it->second.origin.x != p.windowLogical.origin.x ||
               it->second.origin.y != p.windowLogical.origin.y ||
               it->second.size.x != p.windowLogical.size.x ||
               it->second.size.y != p.windowLogical.size.y) {
      it->second = p.windowLogical;
      window.positionSlot(p.id, p.windowLogical);
    }
  }
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
