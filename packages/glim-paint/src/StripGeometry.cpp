#include <algorithm>
#include <cmath>

#include "Strip.h"
#include "StripRows.h"

namespace glim::paint {
namespace {

float scaleOf(const Mat4& t, const Rect& r) {
  if (r.size.x <= kEps || r.size.y <= kEps) {
    const Vec4 o = t * Vec4{0, 0, 0, 1};
    const Vec4 x = t * Vec4{1, 0, 0, 1};
    const Vec4 y = t * Vec4{0, 1, 0, 1};
    const float sx = std::hypot(x.x - o.x, x.y - o.y);
    const float sy = std::hypot(y.x - o.x, y.y - o.y);
    return std::min(sx, sy);
  }
  const Rect xf = transformRect(t, r);
  const float sx = std::fabs(xf.size.x / r.size.x);
  const float sy = std::fabs(xf.size.y / r.size.y);
  return std::min(sx, sy);
}

Radius scaleRadius(Radius r, float s) {
  r.lt *= s;
  r.rt *= s;
  r.lb *= s;
  r.rb *= s;
  return r;
}

}  // namespace

Radius clampRadius(const Rect& rect, Radius r) {
  r.lt = std::max(0.f, r.lt);
  r.rt = std::max(0.f, r.rt);
  r.lb = std::max(0.f, r.lb);
  r.rb = std::max(0.f, r.rb);
  const float w = std::max(0.f, rect.size.x);
  const float h = std::max(0.f, rect.size.y);
  const float top = r.lt + r.rt;
  if (top > w && top > 0.f) {
    const float s = w / top;
    r.lt *= s;
    r.rt *= s;
  }
  const float bot = r.lb + r.rb;
  if (bot > w && bot > 0.f) {
    const float s = w / bot;
    r.lb *= s;
    r.rb *= s;
  }
  const float left = r.lt + r.lb;
  if (left > h && left > 0.f) {
    const float s = h / left;
    r.lt *= s;
    r.lb *= s;
  }
  const float right = r.rt + r.rb;
  if (right > h && right > 0.f) {
    const float s = h / right;
    r.rt *= s;
    r.rb *= s;
  }
  return r;
}

Rect intersectRect(Rect a, Rect b) {
  const float x0 = std::max(a.origin.x, b.origin.x);
  const float y0 = std::max(a.origin.y, b.origin.y);
  const float x1 = std::min(a.origin.x + a.size.x, b.origin.x + b.size.x);
  const float y1 = std::min(a.origin.y + a.size.y, b.origin.y + b.size.y);
  if (x1 <= x0 || y1 <= y0) {
    return {};
  }
  return {{x0, y0}, {x1 - x0, y1 - y0}};
}

Rect strokeBounds(const Stroke& s) {
  const float o = std::max(0.f, s.width) * 0.5f;
  return {
    {s.rect.origin.x - o, s.rect.origin.y - o}, {s.rect.size.x + o * 2.f, s.rect.size.y + o * 2.f}
  };
}

ClipState clipOf(const GroupParams& p, const Mat4& extra) {
  if (!hasClip(p)) {
    return {};
  }
  ClipState c;
  c.active = true;
  c.rect = transformRect(extra, p.clip);
  c.radius = scaleRadius(p.clipRadius, scaleOf(extra, p.clip));
  return c;
}

ClipState intersectClip(const ClipState& parent, const ClipState& child) {
  if (!parent.active) {
    return child;
  }
  if (!child.active) {
    return parent;
  }
  ClipState out;
  out.active = true;
  out.rect = intersectRect(parent.rect, child.rect);
  if (out.rect.size.x <= 0.f || out.rect.size.y <= 0.f) {
    out.active = true;
    out.rect = {};
    return out;
  }
  if (sharp(parent.radius) || sharp(child.radius)) {
    out.radius = {};
  } else {
    out.radius = parent.radius;
    out.radius.lt = std::min(parent.radius.lt, child.radius.lt);
    out.radius.rt = std::min(parent.radius.rt, child.radius.rt);
    out.radius.lb = std::min(parent.radius.lb, child.radius.lb);
    out.radius.rb = std::min(parent.radius.rb, child.radius.rb);
  }
  return out;
}

}  // namespace glim::paint
