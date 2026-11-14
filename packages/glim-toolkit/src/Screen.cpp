#include <functional>
#include <optional>
#include <vector>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/Toolkit.h"
#include "glim/toolkit/elements/Roll.h"

namespace glim {
namespace toolkit {

bool Screen::pointerDown(
  const Block& tree, const Box& box, float x, float y, int pointerId, const Engine& engine
) {
  // Events borrow: the payload must outlive the dispatch.
  const PointerDown press{x, y, clock.now};
  Event down = Event::borrow(press);
  if (dispatchPointer(tree, box, x, y, down, engine)) {
    // Capture only on a focusable hit; a handled non-focusable clears stale capture.
    int hit = focusAt(tree, box, x, y, engine);
    if (hit >= 0) {
      focus = hit;
      drag = hit;
      dragId = pointerId;
    } else {
      drag = -1;
    }
    focusVisible = false;
    return true;
  }
  drag = -1;
  return false;
}

bool Screen::pointerUp(const Block& tree, const Box& box, float x, float y, const Engine& engine) {
  const PointerUp release{x, y, clock.now};
  Event up = Event::borrow(release);
  drag = -1;
  return dispatchPointer(tree, box, x, y, up, engine);
}

bool Screen::pointerMove(
  const Block& tree,
  const Box& box,
  float x,
  float y,
  bool pressed,
  int pointerId,
  const Engine& engine
) {
  if (!pressed) return false;
  PointerMove move{x, y, pressed, clock.now};
  Event moved = Event::borrow(move);
  if (drag >= 0) {
    // Second finger never hijacks the drag.
    if (pointerId != dragId) return false;
    // Routed (not consumed) keeps the drag; stale falls back to hit-test.
    if (dispatchCaptured(tree, box, drag, moved, engine)) return false;
    drag = -1;
  }
  return dispatchPointer(tree, box, x, y, moved, engine);
}

bool Screen::wheel(
  const Block& tree, const Box& box, float x, float y, float dx, float dy, const Engine& engine
) {
  if (dx == 0 && dy == 0) return false;
  struct Candidate {
    std::optional<std::reference_wrapper<const Roll>> roll;
    Box box;
  };
  std::vector<Candidate> found;
  detail::ClipStack clip;
  walk(
    tree,
    box,
    engine,
    [&](const Block& n, const Box& bx, const Engine&) -> bool {
      // Only ancestor-visible points are candidates (own clip pushed below).
      bool parentVisible = clip.visible(x, y);
      clip.enter(n, bx);
      if (parentVisible) {
        if (const Roll* r = rollOf(n)) {
          if (r->scrollOff && detail::boxContains(bx, x, y)) found.push_back({std::cref(*r), bx});
        }
      }
      return true;
    },
    [&](const Block&, const Box&, const Engine&) { clip.exit(); }
  );
  // Innermost first; edge-clamped inner rolls chain outward.
  for (auto it = found.rbegin(); it != found.rend(); ++it) {
    const Roll& r = it->roll->get();
    const Box rb = it->box;
    // Strict axis match: vertical wheels bubble past a horizontal strip.
    const RollMetrics m = rollMetrics(r, rb, engine);
    const float delta = m.horizontal ? dx : dy;
    if (delta == 0.0f || m.maxOff <= 0.0f) continue;
    const float next = detail::clampF(m.offset + delta, 0.0f, m.maxOff);
    if (next != m.offset) {
      r.scrollTo(next);
      // Direct by default; the coast only arms when opted in.
      if (smoothScroll) armFling(r, m, next, delta);
      return true;
    }
  }
  return false;
}

bool Screen::key(const Block& tree, const Box& box, Key key, const Engine& engine) {
  focus = deliverKey(tree, box, focus, key, engine);
  focusVisible = true;
  return true;
}

namespace {

// Release-coast tuning: gesture gap (s), stream blend, velocity cap, fling floor.
constexpr float kWheelGap = 0.1f;
constexpr float kVelKeep = 0.65f;
constexpr float kVelMax = 5000.0f;
constexpr float kFlingMin = 60.0f;

}  // namespace

void Screen::armFling(const Roll& r, const RollMetrics& m, float offset, float delta) {
  const float now = clock.now;
  const float dt = lastWheelAt_ < 0.0f ? -1.0f : now - lastWheelAt_;
  lastWheelAt_ = now;
  // Lone events re-anchor and kill coast: only rapid streams can fling.
  if (dt < 0.0f || dt > kWheelGap) {
    flingVel_ = dt > 0.0f ? delta / dt : 0.0f;
    flinging_ = false;
    flingCell_ = State<float>{};
    return;
  }
  flingVel_ = kVelKeep * flingVel_ + (1.0f - kVelKeep) * (delta / dt);
  if (flingVel_ > kVelMax) flingVel_ = kVelMax;
  if (flingVel_ < -kVelMax) flingVel_ = -kVelMax;
  flingCell_ = r.scrollOff;
  fling_ = flingTo(offset, flingVel_, 0.0f, m.maxOff, now);
  flinging_ = fling_.to != offset && (flingVel_ > kFlingMin || flingVel_ < -kFlingMin);
  if (!flinging_) flingCell_ = State<float>{};
}

bool Screen::tick(const Block& tree, const Box& box, const Engine& engine) {
  // Opt-out settles a live coast at once: nothing strands moving().
  if (!smoothScroll) {
    flinging_ = false;
    flingCell_ = State<float>{};
    return false;
  }
  if (!flinging_) return false;
  bool moved = false;
  {
    const float now = clock.now;
    std::optional<std::reference_wrapper<const Roll>> found;
    Box fb{};
    walk(tree, box, engine, [&](const Block& n, const Box& bx) {
      if (!found) {
        if (const Roll* r = rollOf(n)) {
          if (r->scrollOff == flingCell_) {
            found = std::cref(*r);
            fb = bx;
          }
        }
      }
    });
    if (!found) {
      flinging_ = false;
      flingCell_ = State<float>{};
    } else {
      const RollMetrics m = rollMetrics(found->get(), fb, engine);
      const float next = detail::clampF(fling_.at(now), 0.0f, m.maxOff);
      if (fling_.done(now)) {
        flinging_ = false;
        flingCell_ = State<float>{};
      }
      if (next != m.offset) {
        found->get().scrollTo(next);
        moved = true;
      }
    }
  }
  return moved;
}

bool Screen::needsPaint(const Store& store) const { return dirty.check(store.revision()); }

void Screen::painted(const Store& store) { dirty.acknowledge(store.revision()); }

}  // namespace toolkit
}  // namespace glim
