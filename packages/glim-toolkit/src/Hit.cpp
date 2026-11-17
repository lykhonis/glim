#include <algorithm>
#include <cmath>
#include <ostream>
#include <vector>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/Toolkit.h"

namespace glim {
namespace toolkit {

namespace detail {

bool boxContains(const Box& b, float x, float y) {
  return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

PlaneMap PlaneMap::affine(float a, float b, float c, float d, float tx, float ty) {
  PlaneMap p;
  p.m.m[0] = a, p.m.m[1] = b;
  p.m.m[3] = c, p.m.m[4] = d;
  p.m.m[6] = tx, p.m.m[7] = ty;
  return p;
}

std::pair<float, float> PlaneMap::apply(float x, float y) const {
  const Vec3 q = m * Vec3{x, y, 1.0f};
  if (q.z == 0.0f) return {q.x, q.y};
  return {q.x / q.z, q.y / q.z};
}

Box mapBox(const PlaneMap& m, const Box& b) {
  const auto [x0, y0] = m.apply(b.x, b.y);
  float lx = x0, hx = x0, ly = y0, hy = y0;
  for (const auto& [px, py] :
       {m.apply(b.x + b.w, b.y), m.apply(b.x, b.y + b.h), m.apply(b.x + b.w, b.y + b.h)}) {
    lx = std::min(lx, px), hx = std::max(hx, px);
    ly = std::min(ly, py), hy = std::max(hy, py);
  }
  return {lx, ly, hx - lx, hy - ly};
}

PlaneMap childSpace(const Block& n) {
  PlaneMap m;
  if (auto t = n.tag<TransformTag>()) m = PlaneMap::affine(1, 0, 0, 1, -t->get().dx, -t->get().dy);
  if (auto r = n.tag<RotateTag>()) {
    const RotateTag& rt = r->get();
    const float rad = rt.angleDeg * 3.14159265f / 180.0f;
    const float cs = std::cos(rad);
    const float sn = std::sin(rad);
    const PlaneMap undo = PlaneMap::affine(
      cs, -sn, sn, cs, rt.cx - (cs * rt.cx + sn * rt.cy), rt.cy - (cs * rt.cy - sn * rt.cx)
    );
    m = m.then(undo);
  }
  return m;
}

void PointStack::enter(const Block& n, const Box& bx) {
  const bool root = frames_.empty();
  const PlaneMap base = root ? PlaneMap{} : frames_.back().toChildren;
  const bool visible = root || frames_.back().childrenVisible;
  Frame f{base, base.then(childSpace(n)), visible, visible};
  if (visible && clipsChildren(n)) {
    const auto [cx, cy] = f.toChildren.apply(x_, y_);
    f.childrenVisible = boxContains(bx, cx, cy);
  }
  frames_.push_back(f);
}

}  // namespace detail

HitAt hitWithBox(const Block& b, const Box& box, float x, float y, const Engine& e) {
  HitAt found;
  int matches = 0;
  struct Claim {
    BlockRef node;
    int entered;
  };
  std::vector<Claim> claims;
  detail::AutoCtx::found().clear();
  detail::PointStack points(x, y);
  const auto inside = [&](const Box& bx) {
    const auto [px, py] = points.point();
    return points.visible() && detail::boxContains(bx, px, py);
  };
  const auto take = [&](const Block& n, const Box& bx) {
    found = {std::cref(n), bx, points.toNode()};
    detail::noteHit();
  };
  walk(
    b,
    box,
    e,
    [&](const Block& n, const Box& bx, const Engine&) -> bool {
      points.enter(n, bx);
      if (claimsHit(n)) {
        if (inside(bx)) {
          take(n, bx);
          ++matches;
          return false;
        }
        claims.push_back({std::cref(n), matches});
        return true;
      }
      if (inside(bx) && hitParticipates(n)) {
        take(n, bx);
        ++matches;
      }
      return true;
    },
    [&](const Block& n, const Box& bx, const Engine&) {
      if (!claims.empty() && claims.back().node && &claims.back().node->get() == &n) {
        if (matches > claims.back().entered && points.visible()) take(n, bx);
        claims.pop_back();
      }
      points.exit();
    }
  );
  return found;
}

BlockRef hit(const Block& b, const Box& box, float x, float y, const Engine& e) {
  return hitWithBox(b, box, x, y, e).node;
}

void dumpIn(std::ostream& os, const Block& b, const Box& box, const Engine& e, int indent) {
  auto print = [&](const Rec::Entry& en, const char* mark, int at) {
    for (int i = 0; i < at; ++i) os << "  ";
    os << en.kind << " [" << en.box.x << "," << en.box.y << " " << en.box.w << "x" << en.box.h
       << "]";
    if (!en.text.empty()) os << " " << en.text;
    if (mark[0] != '\0') os << " " << mark;
    os << "\n";
  };
  int depth = indent;
  walk(
    b,
    box,
    e,
    [&](const Block& n, const Box& bx, const Engine&) -> bool {
      const char* mark = n.has<ExpandTag>() ? "[expand]" : "";
      Rec tmp;
      describeSelf(n, bx, tmp);
      if (tmp.entries.empty()) {
        if (!walkHasChildren(n)) {
          for (int i = 0; i < depth; ++i) os << "  ";
          os << "Custom [" << bx.x << "," << bx.y << " " << bx.w << "x" << bx.h << "]";
          if (mark[0] != '\0') os << " " << mark;
          os << "\n";
        }
        ++depth;  // balanced with exit; pruned below
        return false;
      }
      for (const auto& en : tmp.entries) print(en, mark, depth);
      ++depth;
      return walkHasChildren(n);
    },
    [&](const Block&, const Box&, const Engine&) { --depth; }
  );
}

void dump(std::ostream& os, const Block& b, const Box& box, const Engine& e, int indent) {
  dumpIn(os, b, box, e, indent);
}

}  // namespace toolkit
}  // namespace glim
