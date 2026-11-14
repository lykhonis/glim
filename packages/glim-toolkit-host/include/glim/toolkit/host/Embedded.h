#pragma once

#include <glim/paint/Scene.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace glim {
namespace toolkit {
namespace host {

// One native integration mode (K86): Texture, Present or Slot overlay.
// Frame and lifecycle facets; input and a11y join with N4 / N5.
class Embedder {
 public:
  virtual ~Embedder() = default;
  virtual void attach(const paint::SlotPlacement& p) = 0;
  virtual void place(const paint::SlotPlacement& p) = 0;
  virtual void detach(std::uint32_t id) = 0;
};

using EmbedFrames = std::unordered_map<std::uint32_t, Rect>;

// Attaches new ids, places on move only, detaches removed ids.
inline void syncEmbeds(
  Embedder& embedder, const std::vector<paint::SlotPlacement>& placed, EmbedFrames& attached
) {
  EmbedFrames next;
  for (const auto& p : placed) next[p.id] = p.windowLogical;
  for (auto it = attached.begin(); it != attached.end();) {
    if (next.find(it->first) == next.end()) {
      embedder.detach(it->first);
      it = attached.erase(it);
    } else {
      ++it;
    }
  }
  for (const auto& p : placed) {
    auto it = attached.find(p.id);
    if (it == attached.end()) {
      attached[p.id] = p.windowLogical;
      embedder.attach(p);
    } else if (it->second.origin.x != p.windowLogical.origin.x ||
               it->second.origin.y != p.windowLogical.origin.y ||
               it->second.size.x != p.windowLogical.size.x ||
               it->second.size.y != p.windowLogical.size.y) {
      it->second = p.windowLogical;
      embedder.place(p);
    }
  }
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
