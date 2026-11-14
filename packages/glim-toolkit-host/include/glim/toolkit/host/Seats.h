#pragma once

#include <glim/paint/Scene.h>
#include <glim/shell/Slot.h>
#include <glim/shell/Window.h>
#include <glim/toolkit/host/Embedded.h>
#include <glim/toolkit/host/SeatView.h>

#include <cstdint>
#include <vector>

namespace glim {
namespace toolkit {
namespace host {

// Slot mode: an embedder-owned native view over the surface. Last resort (K86).
class SlotOverlay final : public Embedder {
 public:
  SlotOverlay(glim::shell::Window& window, SeatViewFactory makeView)
      : window_(window), makeView_(makeView) {}

  void attach(const paint::SlotPlacement& p) override {
    if (makeView_ != nullptr) {
      if (void* view = makeView_(SeatRequest{p.id, p.windowLogical})) {
        window_.attachSlot(p.id, glim::shell::SlotNative{view});
      }
    }
    window_.positionSlot(p.id, p.windowLogical);
  }
  void place(const paint::SlotPlacement& p) override {
    window_.positionSlot(p.id, p.windowLogical);
  }
  void detach(std::uint32_t id) override { window_.detachSlot(id); }

 private:
  glim::shell::Window& window_;
  SeatViewFactory makeView_;
};

inline void syncSeats(
  glim::shell::Window& window,
  const glim::paint::Scene& scene,
  EmbedFrames& attached,
  SeatViewFactory makeView = &makeDefaultSeatView
) {
  std::vector<glim::paint::SlotPlacement> placed;
  glim::paint::collectSlots(scene, &placed);
  SlotOverlay overlay(window, makeView);
  syncEmbeds(overlay, placed, attached);
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
