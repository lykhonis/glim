#pragma once

#include <cstdint>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/DirtyGate.h"
#include "glim/toolkit/Event.h"
#include "glim/toolkit/Motion.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

class Store;
struct Roll;
struct RollMetrics;

// Retained per-screen routing; pointer identity crosses as an opaque int.
struct Screen {
  DirtyGate dirty;
  int focus = -1;
  // Focus-visible modality: pointer sets focus silently, keyboard shows the ring.
  bool focusVisible = false;
  // Pointer capture: pressed moves route straight to the hit target, plus id so a
  // second touch can't hijack the drag.
  int drag = -1;
  int dragId = 0;
  // Run time, seconds; wheel velocity and fling share this epoch.
  Clock clock;

  // Captured drags return false (Frame tick presents them); hover moves never dispatch.
  bool pointerDown(
      const Block& tree,
      const Box& box,
      float x,
      float y,
      int pointerId,
      const Engine& engine = Engine{}
  );
  bool pointerUp(
      const Block& tree, const Box& box, float x, float y, const Engine& engine = Engine{}
  );
  bool pointerMove(
      const Block& tree,
      const Box& box,
      float x,
      float y,
      bool pressed,
      int pointerId,
      const Engine& engine = Engine{}
  );
  bool key(const Block& tree, const Box& box, Key key, const Engine& engine = Engine{});
  // Fling integrate at clock.now; walk-clamped so a mid-coast resize still settles inside.
  bool tick(const Block& tree, const Box& box, const Engine& engine = Engine{});
  // True while a fling coast is live; host skips rebuild when false.
  bool moving() const { return flinging_; }
  // Routes to the innermost scrollable Roll that can move, then chains outward.
  bool wheel(
      const Block& tree,
      const Box& box,
      float x,
      float y,
      float dx,
      float dy,
      const Engine& engine = Engine{}
  );

  bool needsPaint(const Store& store) const;
  void painted(const Store& store);

 private:
  void armFling(const Roll& r, const RollMetrics& m, float offset, float delta);
  // Release coast: scroll cell under decay plus its tween.
  State<float> flingCell_;
  Tween fling_;
  bool flinging_ = false;
  float flingVel_ = 0.0f;
  float lastWheelAt_ = -1.0f;
};

}  // namespace toolkit
}  // namespace glim
