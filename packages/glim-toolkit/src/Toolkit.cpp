#include "glim/toolkit/Place.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <variant>

#include "glim/toolkit/Toolkit.h"

namespace glim {
namespace toolkit {

namespace detail {

float clampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

Box measureText(const std::string& text, Constraints c) {
  return resolveSize(static_cast<float>(text.size()) * Tokens::kTypeAdvance, Tokens::kTypeLine,
                     c);
}

Box resolveSize(float naturalW, float naturalH, Constraints c) {
  float w = (c.minW == c.maxW) ? c.maxW : (naturalW < c.minW ? c.minW : naturalW);
  float h = (c.minH == c.maxH) ? c.maxH : (naturalH < c.minH ? c.minH : naturalH);
  return {0, 0, w, h};
}

Constraints inset(Constraints c, float dx, float dy) {
  if (!isUnbounded(c.maxW)) {
    c.maxW -= dx;
    if (c.maxW < 0) c.maxW = 0;
  }
  if (!isUnbounded(c.maxH)) {
    c.maxH -= dy;
    if (c.maxH < 0) c.maxH = 0;
  }
  if (c.minW > 0) {
    c.minW -= dx;
    if (c.minW < 0) c.minW = 0;
  }
  if (c.minH > 0) {
    c.minH -= dy;
    if (c.minH < 0) c.minH = 0;
  }
  if (c.minW > c.maxW) c.minW = c.maxW;
  if (c.minH > c.maxH) c.minH = c.maxH;
  return c;
}

namespace {

Constraints childConstraints(Constraints c) { return {0, c.maxW, 0, c.maxH}; }

bool isFillBlock(const Block& b) { return b.get<Fill>() != nullptr; }

}  // namespace

Flow flowRow(const std::vector<Block>& children, int gap, Constraints c, const Engine& e) {
  Flow f;
  std::size_t nFill = 0;
  for (const auto& child : children)
    if (isFillBlock(child)) ++nFill;
  Constraints cc = childConstraints(c);
  if (nFill == 0) {
    float x = 0, h = 0;
    for (const auto& child : children) {
      Box b = e.place(child, cc);
      b.x = x;
      b.y = 0;
      f.children.push_back(b);
      x += b.w + static_cast<float>(gap);
      h = std::max(h, b.h);
    }
    if (!children.empty()) x -= static_cast<float>(gap);
    f.size = {0, 0, clampF(x, c.minW, c.maxW), clampF(h, c.minH, c.maxH)};
    return f;
  }
  // Measure rigid children first; fills split the remainder equally.
  std::vector<Box> rigid(children.size());
  std::vector<bool> rigidDone(children.size(), false);
  float rigidW = 0;
  for (std::size_t i = 0; i < children.size(); ++i) {
    if (isFillBlock(children[i])) continue;
    Box b = e.place(children[i], cc);
    rigid[i] = b;
    rigidDone[i] = true;
    rigidW += b.w;
  }
  float gaps = children.size() > 1 ? static_cast<float>(gap) * (children.size() - 1) : 0.0f;
  bool bounded = !isUnbounded(c.maxW);
  float remainder = bounded ? c.maxW - rigidW - gaps : -1.0f;
  if (!bounded || remainder < 0) {
    // Unbounded axis: fill is a programmer error, fall back to natural.
    // Overflow: rigid budget already blown, fills spill at natural size.
    float x = 0, h = 0;
    for (std::size_t i = 0; i < children.size(); ++i) {
      Box b;
      if (rigidDone[i]) {
        b = rigid[i];
      } else {
        const Fill* fl = children[i].get<Fill>();
        b = e.place(fl->children[0], cc);
      }
      b.x = x;
      b.y = 0;
      f.children.push_back(b);
      x += b.w + static_cast<float>(gap);
      h = std::max(h, b.h);
    }
    if (!children.empty()) x -= static_cast<float>(gap);
    f.size = {0, 0, clampF(x, c.minW, c.maxW), clampF(h, c.minH, c.maxH)};
    return f;
  }
  float share = remainder / static_cast<float>(nFill);
  float x = 0, h = 0;
  for (std::size_t i = 0; i < children.size(); ++i) {
    Box b;
    if (rigidDone[i]) {
      b = rigid[i];
    } else {
      const Fill* fl = children[i].get<Fill>();
      Box inner = e.place(fl->children[0], {share, share, 0, c.maxH});
      b = {0, 0, share, inner.h};
    }
    b.x = x;
    b.y = 0;
    f.children.push_back(b);
    x += b.w + static_cast<float>(gap);
    h = std::max(h, b.h);
  }
  if (!children.empty()) x -= static_cast<float>(gap);
  f.size = {0, 0, clampF(x, c.minW, c.maxW), clampF(h, c.minH, c.maxH)};
  return f;
}

Flow flowColumn(const std::vector<Block>& children, int gap, Constraints c, const Engine& e) {
  Flow f;
  std::size_t nFill = 0;
  for (const auto& child : children)
    if (isFillBlock(child)) ++nFill;
  Constraints cc = childConstraints(c);
  if (nFill == 0) {
    float y = 0, w = 0;
    for (const auto& child : children) {
      Box b = e.place(child, cc);
      b.x = 0;
      b.y = y;
      f.children.push_back(b);
      y += b.h + static_cast<float>(gap);
      w = std::max(w, b.w);
    }
    if (!children.empty()) y -= static_cast<float>(gap);
    f.size = {0, 0, clampF(w, c.minW, c.maxW), clampF(y, c.minH, c.maxH)};
    return f;
  }
  std::vector<Box> rigid(children.size());
  std::vector<bool> rigidDone(children.size(), false);
  float rigidH = 0;
  for (std::size_t i = 0; i < children.size(); ++i) {
    if (isFillBlock(children[i])) continue;
    Box b = e.place(children[i], cc);
    rigid[i] = b;
    rigidDone[i] = true;
    rigidH += b.h;
  }
  float gaps = children.size() > 1 ? static_cast<float>(gap) * (children.size() - 1) : 0.0f;
  bool bounded = !isUnbounded(c.maxH);
  float remainder = bounded ? c.maxH - rigidH - gaps : -1.0f;
  if (!bounded || remainder < 0) {
    float y = 0, w = 0;
    for (std::size_t i = 0; i < children.size(); ++i) {
      Box b;
      if (rigidDone[i]) {
        b = rigid[i];
      } else {
        const Fill* fl = children[i].get<Fill>();
        b = e.place(fl->children[0], cc);
      }
      b.x = 0;
      b.y = y;
      f.children.push_back(b);
      y += b.h + static_cast<float>(gap);
      w = std::max(w, b.w);
    }
    if (!children.empty()) y -= static_cast<float>(gap);
    f.size = {0, 0, clampF(w, c.minW, c.maxW), clampF(y, c.minH, c.maxH)};
    return f;
  }
  float share = remainder / static_cast<float>(nFill);
  float y = 0, w = 0;
  for (std::size_t i = 0; i < children.size(); ++i) {
    Box b;
    if (rigidDone[i]) {
      b = rigid[i];
    } else {
      const Fill* fl = children[i].get<Fill>();
      Box inner = e.place(fl->children[0], {0, c.maxW, share, share});
      b = {0, 0, inner.w, share};
    }
    b.x = 0;
    b.y = y;
    f.children.push_back(b);
    y += b.h + static_cast<float>(gap);
    w = std::max(w, b.w);
  }
  if (!children.empty()) y -= static_cast<float>(gap);
  f.size = {0, 0, clampF(w, c.minW, c.maxW), clampF(y, c.minH, c.maxH)};
  return f;
}

Flow flowStack(const std::vector<Block>& children, Constraints c, const Engine& e) {
  Flow f;
  float w = 0, h = 0;
  for (const auto& child : children) {
    Box b = e.place(child, c);
    b.x = 0;
    b.y = 0;
    f.children.push_back(b);
    w = std::max(w, b.w);
    h = std::max(h, b.h);
  }
  f.size = {0, 0, clampF(w, c.minW, c.maxW), clampF(h, c.minH, c.maxH)};
  return f;
}

}  // namespace detail

Box place(const Block& b, Constraints c) {
  Engine e;
  return std::visit([&](const auto& x) -> Box { return x.place(c, e); }, b.v);
}

void record(const Block& b, const Box& box, Rec& r) {
  Engine e;
  std::visit([&](const auto& x) { x.record(box, r, e); }, b.v);
}

namespace {

const char* kindOf(const Block& b) {
  if (b.get<Row>()) return Row::kKind;
  if (b.get<Column>()) return Column::kKind;
  if (b.get<Stack>()) return Stack::kKind;
  if (b.get<Lines>()) return Lines::kKind;
  if (b.get<Roll>()) return b.get<Roll>()->axis == kAxisHorizontal ? Roll::kKindH : Roll::kKindV;
  if (b.get<Fill>()) return Fill::kKind;
  if (const auto* t = b.get<Tap>()) return t->isPrimary ? Tap::kKindPrimary : Tap::kKind;
  if (b.get<Custom>()) return "Custom";
  return Empty::kKind;
}

bool contains(const Box& b, float x, float y) {
  return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

const Block* hitIn(const Block& b, const Box& box, float x, float y, const Engine& e) {
  if (const auto* r = b.get<Row>()) {
    detail::Flow f = detail::flowRow(r->children, r->gap, {0, box.w, 0, box.h}, e);
    for (std::size_t i = r->children.size(); i-- > 0;) {
      Box cb = f.children[i];
      cb.x += box.x;
      cb.y += box.y;
      if (const Block* t = hitIn(r->children[i], cb, x, y, e)) return t;
    }
    return contains(box, x, y) ? &b : nullptr;
  }
  if (const auto* c = b.get<Column>()) {
    detail::Flow f = detail::flowColumn(c->children, c->gap, {0, box.w, 0, box.h}, e);
    for (std::size_t i = c->children.size(); i-- > 0;) {
      Box cb = f.children[i];
      cb.x += box.x;
      cb.y += box.y;
      if (const Block* t = hitIn(c->children[i], cb, x, y, e)) return t;
    }
    return contains(box, x, y) ? &b : nullptr;
  }
  if (const auto* s = b.get<Stack>()) {
    detail::Flow f = detail::flowStack(s->children, {0, box.w, 0, box.h}, e);
    for (std::size_t i = s->children.size(); i-- > 0;) {
      Box cb = f.children[i];
      cb.x += box.x;
      cb.y += box.y;
      if (const Block* t = hitIn(s->children[i], cb, x, y, e)) return t;
    }
    return contains(box, x, y) ? &b : nullptr;
  }
  if (const auto* rl = b.get<Roll>()) {
    float off = (rl->scrollOff ? *rl->scrollOff : 0.0f);
    detail::Flow f;
    if (rl->axis == kAxisHorizontal) {
      Constraints unc{0, kUnbounded, 0, box.h};
      f = detail::flowRow(rl->children, rl->gap, unc, e);
      for (std::size_t i = rl->children.size(); i-- > 0;) {
        Box cb = f.children[i];
        cb.x += box.x - off;
        cb.y += box.y;
        if (const Block* t = hitIn(rl->children[i], cb, x, y, e)) return t;
      }
    } else {
      Constraints unc{0, box.w, 0, kUnbounded};
      f = detail::flowColumn(rl->children, rl->gap, unc, e);
      for (std::size_t i = rl->children.size(); i-- > 0;) {
        Box cb = f.children[i];
        cb.x += box.x;
        cb.y += box.y - off;
        if (const Block* t = hitIn(rl->children[i], cb, x, y, e)) return t;
      }
    }
    return contains(box, x, y) ? &b : nullptr;
  }
  if (const auto* fl = b.get<Fill>()) return hitIn(fl->children[0], box, x, y, e);
  if (const auto* t = b.get<Tap>()) {
    if (hitIn(t->children[0], t->innerBox(box), x, y, e)) return &b;
    return contains(box, x, y) ? &b : nullptr;
  }
  if (const auto* c = b.get<Custom>()) {
    // One walk via visit: child references stay alive only during the
    // callback, so no pointer is retained past a synthesizing eachChild.
    const Block* found = nullptr;
    // Reverse order for last-wins: visit is forward, so track the last hit.
    c->visit(box, e, [&](const Block& child, Box cb) {
      if (const Block* t = hitIn(child, cb, x, y, e)) found = t;
    });
    if (found) return found;
  }
  return contains(box, x, y) ? &b : nullptr;
}

}  // namespace

const Block* hit(const Block& b, const Box& box, float x, float y) {
  Engine e;
  return hitIn(b, box, x, y, e);
}

void dump(std::ostream& os, const Block& b, int indent) {
  // Structural view: variant arms only. Custom elements stay opaque here
  // even when transparent to hit/focus — the record entries (below) are the
  // semantic view that reveals composed children (Card → Lines/Row/Buttons).
  // The two views intentionally differ; see Rec in Layout.h.
  for (int i = 0; i < indent; ++i) os << "  ";
  os << kindOf(b);
  if (const auto* l = b.get<Lines>()) os << " \"" << l->text << "\"";
  os << "\n";
  auto dumpChildren = [&](const std::vector<Block>& children) {
    for (const auto& child : children) dump(os, child, indent + 1);
  };
  if (const auto* r = b.get<Row>()) dumpChildren(r->children);
  if (const auto* c = b.get<Column>()) dumpChildren(c->children);
  if (const auto* s = b.get<Stack>()) dumpChildren(s->children);
  if (const auto* rl = b.get<Roll>()) dumpChildren(rl->children);
  if (const auto* f = b.get<Fill>()) dump(os, f->children[0], indent + 1);
  if (const auto* t = b.get<Tap>()) dump(os, t->children[0], indent + 1);
}

bool fire(const Block& b) {
  if (const auto* t = b.get<Tap>()) return t->fire();
  if (const auto* f = b.get<Fill>()) return fire(f->children[0]);
  if (const auto* c = b.get<Custom>()) return c->fire();
  return false;
}

namespace {

struct FocusedBox {
  const Block* block;
  Box box;
};

void collectFocus(const Block& b, const Box& box, const Engine& e, std::vector<FocusedBox>& out) {
  if (b.get<Tap>()) {
    out.push_back({&b, box});
    return;
  }
  if (const auto* c = b.get<Custom>()) {
    // Fireable containers stay reachable themselves (pre-order) plus their
    // children; pure containers recurse; opaque leaves push iff fireable.
    bool transparent = c->childCount() > 0;
    if (c->fireable()) out.push_back({&b, box});
    if (transparent) {
      c->visit(box, e, [&](const Block& child, Box cb) { collectFocus(child, cb, e, out); });
    }
    return;
  }
  if (const auto* f = b.get<Fill>()) {
    collectFocus(f->children[0], box, e, out);
    return;
  }
  auto flowed = [&](const std::vector<Block>& children, const detail::Flow& f, float dx,
                     float dy) {
    for (std::size_t i = 0; i < children.size(); ++i) {
      Box cb = f.children[i];
      cb.x += box.x + dx;
      cb.y += box.y + dy;
      collectFocus(children[i], cb, e, out);
    }
  };
  if (const auto* r = b.get<Row>()) {
    flowed(r->children, detail::flowRow(r->children, r->gap, {0, box.w, 0, box.h}, e), 0, 0);
  } else if (const auto* c = b.get<Column>()) {
    flowed(c->children, detail::flowColumn(c->children, c->gap, {0, box.w, 0, box.h}, e), 0, 0);
  } else if (const auto* s = b.get<Stack>()) {
    flowed(s->children, detail::flowStack(s->children, {0, box.w, 0, box.h}, e), 0, 0);
  } else if (const auto* rl = b.get<Roll>()) {
    float off = (rl->scrollOff ? *rl->scrollOff : 0.0f);
    if (rl->axis == kAxisHorizontal) {
      flowed(rl->children, detail::flowRow(rl->children, rl->gap, {0, kUnbounded, 0, box.h}, e),
             -off, 0);
    } else {
      flowed(rl->children, detail::flowColumn(rl->children, rl->gap, {0, box.w, 0, kUnbounded}, e),
             0, -off);
    }
  }
}

}  // namespace

std::vector<const Block*> focusables(const Block& root, const Box& box) {
  Engine e;
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  std::vector<const Block*> out;
  for (const auto& f : found) out.push_back(f.block);
  return out;
}

Box focusBox(const Block& root, const Box& box, int index) {
  Engine e;
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  if (index < 0 || static_cast<std::size_t>(index) >= found.size()) return {0, 0, 0, 0};
  return found[static_cast<std::size_t>(index)].box;
}

int moveFocus(const Block& root, const Box& box, int current, FocusDir dir) {
  Engine e;
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  if (found.empty()) return 0;
  int n = static_cast<int>(found.size());
  if (current < 0) current = 0;
  if (current >= n) current = n - 1;
  if (dir == FocusDir::Next) return current + 1 < n ? current + 1 : current;
  if (dir == FocusDir::Prev) return current > 0 ? current - 1 : current;
  auto center = [](const Box& b) -> std::pair<float, float> {
    return {b.x + b.w * 0.5f, b.y + b.h * 0.5f};
  };
  auto [cx, cy] = center(found[static_cast<std::size_t>(current)].box);
  int best = current;
  float bestScore = 0;
  bool hasBest = false;
  for (int i = 0; i < n; ++i) {
    if (i == current) continue;
    auto [x, y] = center(found[static_cast<std::size_t>(i)].box);
    float dx = x - cx, dy = y - cy;
    bool beyond = false;
    float primary = 0, secondary = 0;
    switch (dir) {
      case FocusDir::Right: beyond = dx > 0.5f; primary = dx; secondary = std::fabs(dy); break;
      case FocusDir::Left: beyond = dx < -0.5f; primary = -dx; secondary = std::fabs(dy); break;
      case FocusDir::Down: beyond = dy > 0.5f; primary = dy; secondary = std::fabs(dx); break;
      case FocusDir::Up: beyond = dy < -0.5f; primary = -dy; secondary = std::fabs(dx); break;
      default: break;
    }
    if (!beyond) continue;
    float score = primary + 4.0f * secondary;
    if (!hasBest || score < bestScore) {
      hasBest = true;
      bestScore = score;
      best = i;
    }
  }
  return best;
}

}  // namespace toolkit
}  // namespace glim
