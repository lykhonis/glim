#pragma once

#include <cstdint>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/DirtyGate.h"
#include "glim/toolkit/Event.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

class Store;

// Retained per-screen interaction state + routing. The app owns one
// (beside its Store) and hands it to the host loop; record functions
// read focus/focusVisible for the ring. Shell-free, paint-free: pointer
// identity crosses as a plain int (shell concept, toolkit treats it as
// opaque), keys arrive already mapped, pixels never appear.
struct Screen {
  DirtyGate dirty;
  int focus = -1;
  // Focus-visible modality: pointer sets focus silently, keyboard
  // shows the ring.
  bool focusVisible = false;
  // Pointer capture: PointerDown stores the hit focusable; pressed
  // moves route straight to it (no re-hit-test) so a drag never drops
  // out when the finger leaves the box, plus the pointer id so a
  // second touch can't hijack the drag.
  int drag = -1;
  int dragId = 0;

  // Returns true when the caller should paint immediately. Captured
  // drags return false (the Frame tick presents them: a synchronous
  // present per drag event blocks on vsync and queues input behind
  // it); hover moves never dispatch.
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

  bool needsPaint(const Store& store) const;
  void painted(const Store& store);
};

}  // namespace toolkit
}  // namespace glim
