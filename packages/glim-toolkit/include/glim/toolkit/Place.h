#pragma once

#include <cmath>
#include <iosfwd>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include "glim/math.h"
#include "glim/toolkit/Event.h"
#include "glim/toolkit/Layout.h"
#include "glim/toolkit/State.h"

namespace glim {
namespace toolkit {

struct Block;

using BlockRef = std::optional<std::reference_wrapper<const Block>>;

struct Env {
  const void* ptr = nullptr;
  const std::type_info* type = nullptr;
  std::optional<std::reference_wrapper<const Env>> parent;
};

Box place(const Block& b, Constraints c);

bool handle(const Block& b, const Event& ev);

bool handleAt(const Block& b, const Box& box, const Event& ev);

struct Engine {
  std::optional<std::reference_wrapper<const Env>> env;
  StoreRef store;
  Box place(const Block& b, Constraints c) const;
  void record(const Block& b, const Box& box, Rec& r) const;
  template <class T>
  std::optional<std::reference_wrapper<const T>> get() const {
    for (auto p = env; p; p = p->get().parent) {
      const Env& n = p->get();
      if (n.type && *n.type == typeid(T)) return std::cref(*static_cast<const T*>(n.ptr));
    }
    return std::nullopt;
  }
};

// Walk engine must match placement engine; bare Engine{} is the fixed-advance fallback.
void record(const Block& b, const Box& box, Rec& r, const Engine& e = Engine{});
// Lifetime: pointer borrows from `b`; never store past the tree or call on a temporary.
BlockRef hit(const Block& b, const Box& box, float x, float y, const Engine& e = Engine{});
namespace detail {

// x' = a x + c y + tx, y' = b x + d y + ty.
// A projective map of the plane: translate, rotate and perspective tilt
// all compose here, so points hit what paint drew.
struct PlaneMap {
  Mat3 m = Mat3::identity();
  static PlaneMap affine(float a, float b, float c, float d, float tx, float ty);
  std::pair<float, float> apply(float x, float y) const;
  // This map, then `next`.
  PlaneMap then(const PlaneMap& next) const { return {next.m * m}; }
  // Identity when singular (an edge-on tilt hits nothing behind it anyway).
  PlaneMap inverse() const { return {m.inverse().value_or(Mat3::identity())}; }
};
// Axis-aligned bounds of `b` under `m` (a rotated box's enclosing rect).
Box mapBox(const PlaneMap& m, const Box& b);

}  // namespace detail

// Hit target, its laid-out box, and window -> its paint space (translate
// and rotate tags of its ancestors undone). Null node + zero box on miss.
struct HitAt {
  BlockRef node;
  Box box{0, 0, 0, 0};
  detail::PlaneMap toNode;
};

HitAt hitWithBox(const Block& b, const Box& box, float x, float y, const Engine& e = Engine{});

bool dispatchPointer(
  const Block& b, const Box& box, float x, float y, const Event& ev, const Engine& e = Engine{}
);

bool broadcastEvent(const Block& b, const Box& box, const Event& ev, const Engine& e = Engine{});

void dump(
  std::ostream& os, const Block& b, const Box& box, const Engine& e = Engine{}, int indent = 0
);

namespace detail {

float clampF(float v, float lo, float hi);
// One home for hit/scroll geometry (Hit.cpp, Screen::wheel share it).
bool boxContains(const Box& b, float x, float y);
// Window -> the space a node's children paint in: inverse of its translate
// and rotate tags (Emit pushes them after the node paints itself).
PlaneMap childSpace(const Block& n);
// Pointer at (x, y) carried down the walk into each node's paint space, with
// ancestor clips applied: Roll children hit only inside the clip.
class PointStack {
 public:
  PointStack(float x, float y) : x_(x), y_(y) {}
  void enter(const Block& n, const Box& bx);
  void exit() { frames_.pop_back(); }
  const PlaneMap& toNode() const { return frames_.back().toNode; }
  std::pair<float, float> point() const { return toNode().apply(x_, y_); }
  // Inside every ancestor clip (the node's own clip bounds only its children).
  bool visible() const { return frames_.back().visible; }

 private:
  struct Frame {
    PlaneMap toNode;
    PlaneMap toChildren;
    bool visible;
    bool childrenVisible;
  };
  float x_;
  float y_;
  std::vector<Frame> frames_;
};
// handleAt with the event's point mapped by toNode.
bool handleIn(const Block& b, const Box& box, const PlaneMap& toNode, const Event& ev);
// 0.5 logical snaps 1px chrome at 2x; layout boxes stay untouched.
inline float snapHalf(float v) { return std::floor(v * 2.0f + 0.5f) / 2.0f; }
// Tight proposal forces exact size; loose max never truncates.
Box resolveSize(float naturalW, float naturalH, Constraints c);
// Shrink a proposal by pad chrome: max/min reduced by d (floored at 0, unbounded stays unbounded).
Constraints inset(Constraints c, float dx, float dy);

struct Flow {
  Box size;
  std::vector<Box> children;
};

inline std::vector<Box> offsetBoxes(std::vector<Box> boxes, float dx, float dy) {
  for (Box& b : boxes) {
    b.x += dx;
    b.y += dy;
  }
  return boxes;
}

// Opt-in cross-axis centering; place and childBox agree from one call each.
inline void centerRow(Flow& f) {
  for (auto& b : f.children) b.y += (f.size.h - b.h) * 0.5f;
}
inline void centerColumn(Flow& f) {
  for (auto& b : f.children) b.x += (f.size.w - b.w) * 0.5f;
}

Flow flowRow(const std::vector<Block>& children, int gap, Constraints c, const Engine& e);
Flow flowColumn(const std::vector<Block>& children, int gap, Constraints c, const Engine& e);
Flow flowStack(const std::vector<Block>& children, Constraints c, const Engine& e);

}  // namespace detail

}  // namespace toolkit
}  // namespace glim
