#pragma once

// Block tree → sink emission on the canonical walk; sink-agnostic.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Focus.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Tokens.h"
#include "glim/toolkit/Walk.h"
#include "glim/toolkit/elements/Seat.h"
#include "glim/toolkit/theme/Keys.h"

namespace glim {
namespace toolkit {

namespace detail {

template <class S, class F>
void emitTree(const Block& b, const Box& box, const Engine& e, S& sink, F& feed) {
  std::vector<BlockRef> clips;
  std::vector<BlockRef> transforms;
  walk(
      b,
      box,
      e,
      [&](const Block& n, const Box& bx, const Engine& en) -> bool {
        // Seat reserves a host native: structural slot, never an entry; Seats are leaves.
        if (auto seat = n.get<Seat>()) {
          sink.slot(bx, seat->get().id);
          return false;
        }
        // Shadow wraps self + children: push before paintSelf so the
        // card fill itself lands in the shadow group.
        if (auto sh = n.tag<ShadowTag>()) {
          const auto& s = sh->get();
          sink.pushShadow(s.sigma, s.offsetX, s.offsetY, s.color, s.expandPx, s.opacity);
          transforms.push_back(std::cref(n));
        }
        if (auto fx = n.tag<EffectTag>()) {
          const auto& f = fx->get();
          sink.pushEffect(bx, f.radius, static_cast<std::uint32_t>(f.kind), f.color, f.intensity);
          transforms.push_back(std::cref(n));
        }
        paintSelf(n, bx, sink, feed, en);
        // Glass tags go on fill-only containers; text inside glass would frost.
        // Edge mask rides the group so flat plates skip the rim/hairline.
        if (auto g = n.tag<GlassTag>()) {
          sink.pushGlass(
              bx,
              g->get().radius,
              g->get().clear,
              static_cast<std::uint8_t>(g->get().edge),
              g->get().tint,
              static_cast<std::uint8_t>(g->get().material)
          );
          sink.pop();
        }
        if (auto t = n.tag<TransformTag>()) {
          sink.pushTransform(t->get().dx, t->get().dy);
          transforms.push_back(std::cref(n));
        }
        if (auto rt = n.tag<RotateTag>()) {
          sink.pushRotate(rt->get().angleDeg, rt->get().cx, rt->get().cy);
          transforms.push_back(std::cref(n));
        }
        if (clipsChildren(n)) {
          sink.pushClip(bx);
          clips.push_back(std::cref(n));
        }
        return true;
      },
      [&](const Block& n, const Box&, const Engine&) {
        if (!clips.empty() && clips.back() && &clips.back()->get() == &n) {
          clips.pop_back();
          sink.pop();
        }
        // A node may hold both TransformTag and RotateTag (two groups);
        // drain all of them so scopes never leak across siblings.
        while (!transforms.empty() && transforms.back() && &transforms.back()->get() == &n) {
          transforms.pop_back();
          sink.pop();
        }
      }
  );
}

}  // namespace detail

template <class S, class F>
void paintTree(const Block& b, const Box& box, const Engine& e, S& sink, F& feed) {
  detail::emitTree(b, box, e, sink, feed);
}

// Focus ring is a paint-time overlay: paint the tree first, then the ring last.
template <class S>
void paintFocusRing(
    const Block& root,
    const Box& box,
    int index,
    S& sink,
    const Engine& e = Engine{},
    bool visible = true
) {
  if (!visible) return;
  Box rb = focusRingBox(root, box, index, 2.0f, e);
  if (rb.w <= 0 || rb.h <= 0) return;
  sink.stroke(rb, 4, 2.0f, themeColor(e, kThemeFocusRing));
}

}  // namespace toolkit
}  // namespace glim
