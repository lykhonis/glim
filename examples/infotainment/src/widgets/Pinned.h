#pragma once

// Pinned board: children at measured rects of a fixed-size design frame
// (the reference cluster), adapted to the window by anchors. The board
// fills the window; each pin keeps its design size and slides with its
// anchor (left column hugs the left edge, right column the right edge,
// center items stay centered), and the whole frame centers vertically.
//
// Pins may carry a glass backing and a toolkit drop shadow. Shadows
// paint first, then every backing, so glass groups stay consecutive and
// card content lands in the crisp overlay instead of under the frost.

#include <optional>
#include <utility>
#include <vector>

#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

enum class Anchor : std::uint8_t { Start, Center, End };

struct Pin {
  Box at;
  Block block;
  Anchor anchor = Anchor::Center;
  std::optional<GlassTag> backing = std::nullopt;
  // Backing rect in design coordinates; empty uses `at`.
  Box backingAt{};
  // Toolkit drop shadow for the plate. The caster is not drawn: the
  // visible card is the glass backing, which has no alpha of its own.
  std::optional<ShadowTag> shadow = std::nullopt;
};

// Fill the proposal when it is bounded, else keep the design size.
inline Box fillProposal(float designW, float designH, Constraints c) {
  return {
      0.f,
      0.f,
      isUnbounded(c.maxW) ? designW : c.maxW,
      isUnbounded(c.maxH) ? designH : c.maxH
  };
}

struct Pinned {
  static constexpr const char* kKind = "Pinned";
  float width = 0.f;
  float height = 0.f;
  std::vector<Block> children;
  std::vector<Box> rects;
  std::vector<Anchor> anchors;
  std::vector<std::optional<GlassTag>> backings;
  std::vector<Box> backingRects;
  std::vector<std::optional<ShadowTag>> shadows;
  Box place(Constraints c, const Engine&) const {
    return fillProposal(width, height, c);
  }
  // Design rect -> window rect for a pin's anchor.
  Box adapt(const Box& r, Anchor a, const Box& box) const {
    const float slack = box.w - width;
    const float dx = a == Anchor::Start ? 0.f
                     : a == Anchor::End ? slack
                                        : slack * 0.5f;
    return {
        box.x + r.x + dx,
        box.y + r.y + (box.h - height) * 0.5f,
        r.w,
        r.h
    };
  }
  Box childBox(std::size_t i, const Box& box, const Engine&) const {
    return adapt(rects[i], anchors[i], box);
  }
  void record(const Box& box, Rec& rec) const { rec.push(kKind, box); }
  void paint(const Box& box, AnySink& sink, const Engine&) const {
    // Shadows before glass: under-layer isolates, then one consecutive
    // glass run. Opacity 0 keeps the silhouette out of the backdrop the
    // plates sample; the shadow blit is not faded.
    for (std::size_t i = 0; i < rects.size(); ++i) {
      if (!backings[i] || !shadows[i]) continue;
      const GlassTag& g = *backings[i];
      const ShadowTag& s = *shadows[i];
      const Box& r =
          backingRects[i].w > 0.f ? backingRects[i] : rects[i];
      const Box plate = adapt(r, anchors[i], box);
      sink.pushShadow(
          s.sigma, s.offsetX, s.offsetY, s.color, s.expandPx, 0.f
      );
      sink.fillRounded(plate, g.radius, 0xffffffffu);
      sink.pop();
    }
    for (std::size_t i = 0; i < rects.size(); ++i) {
      if (!backings[i]) continue;
      const GlassTag& g = *backings[i];
      const Box& r =
          backingRects[i].w > 0.f ? backingRects[i] : rects[i];
      sink.pushGlass(
          adapt(r, anchors[i], box),
          g.radius,
          g.clear,
          static_cast<std::uint8_t>(g.edge),
          g.tint,
          static_cast<std::uint8_t>(g.material)
      );
      sink.pop();
    }
  }
};

struct PinnedLayout {
  float width = 0.f;
  float height = 0.f;
  std::vector<Pin> pins;
};

inline Block pinned(PinnedLayout layout) {
  Pinned p;
  p.width = layout.width;
  p.height = layout.height;
  for (Pin& pin : layout.pins) {
    p.rects.push_back(pin.at);
    p.anchors.push_back(pin.anchor);
    p.backings.push_back(pin.backing);
    p.backingRects.push_back(pin.backingAt);
    p.shadows.push_back(pin.shadow);
    p.children.push_back(std::move(pin.block));
  }
  return finish(std::move(p));
}

}  // namespace toolkit
}  // namespace glim
