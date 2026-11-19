#pragma once

#include <cstdint>
#include <optional>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/DirtyGate.h"
#include "glim/toolkit/Event.h"
#include "glim/toolkit/Focus.h"
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
  // When the caret last moved (press, key, text); it shows steadily for a
  // beat after, then blinks. Reduce motion keeps it on.
  float caretSince = 0.0f;
  bool caretOn(bool reduceMotion) const;
  // Where the edit menu is open, in window space; any press closes it.
  std::optional<Vec2> menuAt;
  // Pointer capture: pressed moves route straight to the hit target, plus id so a
  // second touch can't hijack the drag.
  int drag = -1;
  int dragId = 0;
  // Run time, seconds; wheel velocity and fling share this epoch.
  Clock clock;
  // Direct scrolling by default; opt in for fling coast on rapid
  // wheel streams.
  bool smoothScroll = false;

  // Captured drags return false (Frame tick presents them); hover moves never dispatch.
  bool pointerDown(
    const Block& tree,
    const Box& box,
    float x,
    float y,
    int pointerId,
    const Engine& engine = Engine{},
    bool shift = false
  );
  // A secondary press (right-click): places the caret like a press unless it
  // lands in the selection, then opens the edit menu if a field has focus.
  bool contextPress(
    const Block& tree, const Box& box, float x, float y, const Engine& engine = Engine{}
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
  // A shell's text change for the focused field; false when focus edits no text.
  bool text(
    const Block& tree, const Box& box, const TextChange& change, const Engine& engine = Engine{}
  );
  std::optional<TextFocus> textFocus(
    const Block& tree, const Box& box, const Engine& engine = Engine{}
  ) const;
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
