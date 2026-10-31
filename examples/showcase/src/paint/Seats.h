#pragma once

// Frame reconcile: toolkit Seat rects → host native views. Collects
// SlotHole placements from the finished Scene, attaches a native view
// per new id (per-platform `glimMakeSeatView`), repositions on move
// only, detaches removed ids. Smoke-only: no pixels asserted, the
// golden path never calls this (slot holes only).
#include <glim/paint/Scene.h>
#include <glim/shell/Slot.h>
#include <glim/shell/Window.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace glim {
namespace toolkit {
namespace paint {

// Per-platform factory (SeatView.mm on macOS, SeatView.cpp stub
// elsewhere returning null — track-only, no view).
void* glimMakeSeatView(std::uint32_t id);

inline void syncSeats(
    glim::shell::Window& window,
    const glim::paint::Scene& scene,
    std::unordered_map<std::uint32_t, glim::Rect>& attached
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
      if (void* view = glimMakeSeatView(p.id))
        window.attachSlot(p.id, glim::shell::SlotNative{view});
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

}  // namespace paint
}  // namespace toolkit
}  // namespace glim
