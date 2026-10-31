#include "glim/toolkit/Toolkit.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <sstream>
#include <type_traits>
#include <variant>

#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

namespace detail {

float clampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

Box measureText(const std::string& text, Constraints c) {
  return resolveSize(static_cast<float>(text.size()) * Tokens::kTypeAdvance, Tokens::kTypeLine, c);
}

std::vector<std::string> wrapText(const std::string& text, float maxW) {
  if (isUnbounded(maxW)) return {text};
  float adv = Tokens::kTypeAdvance;
  int per = static_cast<int>(maxW / adv);
  if (per < 1) per = 1;
  // Split on spaces (collapse runs); empty text is one empty line.
  std::vector<std::string> words;
  std::string cur;
  for (char ch : text) {
    if (ch == ' ') {
      if (!cur.empty()) {
        words.push_back(cur);
        cur.clear();
      }
    } else {
      cur.push_back(ch);
    }
  }
  if (!cur.empty()) words.push_back(cur);
  if (words.empty()) return {""};
  std::vector<std::string> lines;
  std::string line;
  float lineW = 0;
  auto flushWord = [&](const std::string& w) {
    float wW = static_cast<float>(w.size()) * adv;
    if (line.empty()) {
      line = w;
      lineW = wW;
    } else if (lineW + adv + wW <= maxW + 0.001f) {
      line += ' ';
      line += w;
      lineW += adv + wW;
    } else {
      lines.push_back(line);
      line = w;
      lineW = wW;
    }
  };
  for (const auto& w : words) {
    if (static_cast<float>(w.size()) * adv <= maxW + 0.001f) {
      flushWord(w);
    } else {
      // Long word: chunk at char boundaries.
      std::size_t k = 0;
      while (k < w.size()) {
        std::size_t n = static_cast<std::size_t>(per);
        if (n > w.size() - k) n = w.size() - k;
        flushWord(w.substr(k, n));
        k += n;
      }
    }
  }
  if (!line.empty()) lines.push_back(line);
  if (lines.empty()) lines.push_back("");
  return lines;
}

std::vector<std::string> truncateLines(std::vector<std::string> lines, float maxW, float maxH) {
  float lineH = Tokens::kTypeLine;
  float adv = Tokens::kTypeAdvance;
  int maxLines = static_cast<int>(maxH / lineH);
  if (maxLines < 1) maxLines = 1;
  if (static_cast<int>(lines.size()) <= maxLines) return lines;
  lines.resize(static_cast<std::size_t>(maxLines));
  std::string& last = lines.back();
  const std::string dots = "...";
  float dotsW = static_cast<float>(dots.size()) * adv;
  float lastW = static_cast<float>(last.size()) * adv;
  if (lastW + dotsW <= maxW + 0.001f) {
    last += dots;
    return lines;
  }
  int per = isUnbounded(maxW) ? static_cast<int>(last.size()) : static_cast<int>(maxW / adv);
  if (per < 0) per = 0;
  int keep = per - static_cast<int>(dots.size());
  if (keep < 0) {
    // Narrower than "...": as many dots as fit.
    int n = per < 0 ? 0 : per;
    last = dots.substr(0, static_cast<std::size_t>(n));
    return lines;
  }
  if (keep > static_cast<int>(last.size())) keep = static_cast<int>(last.size());
  last = last.substr(0, static_cast<std::size_t>(keep)) + dots;
  return lines;
}

std::vector<std::string> layoutLines(const std::string& text, float maxW, float maxH, bool wrap) {
  std::vector<std::string> lines = wrap ? wrapText(text, maxW) : std::vector<std::string>{text};
  if (isUnbounded(maxH)) return lines;
  return truncateLines(std::move(lines), maxW, maxH);
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

}  // namespace

Flow flowRow(const std::vector<Block>& children, int gap, Constraints c, const Engine& e) {
  Flow f;
  std::size_t nFlex = 0;
  for (const auto& child : children)
    if (child.has<ExpandTag>()) ++nFlex;
  Constraints cc = childConstraints(c);
  if (nFlex == 0) {
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
  // Measure rigid children first; flexible children split the remainder equally.
  std::vector<Box> rigid(children.size());
  std::vector<bool> rigidDone(children.size(), false);
  float rigidW = 0;
  for (std::size_t i = 0; i < children.size(); ++i) {
    if (children[i].has<ExpandTag>()) continue;
    Box b = e.place(children[i], cc);
    rigid[i] = b;
    rigidDone[i] = true;
    rigidW += b.w;
  }
  float gaps = children.size() > 1 ? static_cast<float>(gap) * (children.size() - 1) : 0.0f;
  bool bounded = !isUnbounded(c.maxW);
  float remainder = bounded ? c.maxW - rigidW - gaps : -1.0f;
  if (!bounded || remainder < 0) {
    // Unbounded axis or rigid overflow: flexible children take natural size and spill.
    float x = 0, h = 0;
    for (std::size_t i = 0; i < children.size(); ++i) {
      Box b = e.place(children[i], cc);
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
  float share = remainder / static_cast<float>(nFlex);
  float x = 0, h = 0;
  for (std::size_t i = 0; i < children.size(); ++i) {
    Box b;
    if (rigidDone[i]) {
      b = rigid[i];
    } else {
      Box inner = e.place(children[i], {share, share, 0, c.maxH});
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
  std::size_t nFlex = 0;
  for (const auto& child : children)
    if (child.has<ExpandTag>()) ++nFlex;
  Constraints cc = childConstraints(c);
  if (nFlex == 0) {
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
    if (children[i].has<ExpandTag>()) continue;
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
      Box b = e.place(children[i], cc);
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
  float share = remainder / static_cast<float>(nFlex);
  float y = 0, w = 0;
  for (std::size_t i = 0; i < children.size(); ++i) {
    Box b;
    if (rigidDone[i]) {
      b = rigid[i];
    } else {
      Box inner = e.place(children[i], {0, c.maxW, share, share});
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

Box Engine::place(const Block& b, Constraints c) const {
  return std::visit([&](const auto& x) -> Box { return x.place(c, *this); }, b.v);
}

void Engine::record(const Block& b, const Box& box, Rec& r) const {
  std::visit([&](const auto& x) { x.record(box, r, *this); }, b.v);
}

Box place(const Block& b, Constraints c) { return Engine{}.place(b, c); }

void record(const Block& b, const Box& box, Rec& r) { Engine{}.record(b, box, r); }

namespace {

const char* kindOf(const Block& b) {
  return std::visit([](const auto& x) -> const char* { return x.kind(); }, b.v);
}

bool contains(const Box& b, float x, float y) {
  return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

const Block* hitIn(const Block& b, const Box& box, float x, float y, const Engine& e) {
  if (const auto* c = b.get<Custom>()) {
    const Block* found = nullptr;
    c->visit(box, e, [&](const Block& child, Box cb) {
      if (const Block* t = hitIn(child, cb, x, y, e)) found = t;
    });
    if (found) return found;
    return contains(box, x, y) ? &b : nullptr;
  }
  return std::visit(
      [&](const auto& el) -> const Block* {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_inner_box<T>::value) {
          if (hitIn(el.children[0], el.innerBox(box), x, y, e)) return &b;
          return contains(box, x, y) ? &b : nullptr;
        } else if constexpr (has_children<T>::value) {
          const Block* found = nullptr;
          std::size_t i = 0;
          el.eachChild([&](const Block& child) {
            if (const Block* t = hitIn(child, el.childBox(i++, box, e), x, y, e)) found = t;
          });
          if (found) return found;
          return contains(box, x, y) ? &b : nullptr;
        } else {
          return contains(box, x, y) ? &b : nullptr;
        }
      },
      b.v
  );
}

}  // namespace

const Block* hit(const Block& b, const Box& box, float x, float y) {
  Engine e;
  return hitIn(b, box, x, y, e);
}

void dump(std::ostream& os, const Block& b, int indent) {
  for (int i = 0; i < indent; ++i) os << "  ";
  os << kindOf(b);
  if (b.has<ExpandTag>()) os << " [expand]";
  if (const auto* l = b.get<Lines>()) os << " \"" << l->text << "\"";
  os << "\n";
  auto dumpChildren = [&](const auto& el) {
    el.eachChild([&](const Block& child) { dump(os, child, indent + 1); });
  };
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_children<T>::value) dumpChildren(el);
      },
      b.v
  );
}

bool handle(const Block& b, const Event& ev) {
  if (const auto* c = b.get<Custom>()) return c->handle(ev);
  bool consumed = false;
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_handle<T>::value) consumed = el.handle(ev);
      },
      b.v
  );
  return consumed;
}

bool dispatchPointer(const Block& b, const Box& box, float x, float y, const Event& ev) {
  Engine e;
  const Block* t = hitIn(b, box, x, y, e);
  if (!t) return false;
  return handle(*t, ev);
}

bool broadcastEvent(const Block& b, const Box& box, const Event& ev) {
  Engine e;
  if (handle(b, ev)) return true;
  bool consumed = false;
  if (const auto* c = b.get<Custom>()) {
    c->visit(box, e, [&](const Block& child, Box cb) {
      if (broadcastEvent(child, cb, ev)) consumed = true;
    });
    return consumed;
  }
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_children<T>::value) {
          std::size_t i = 0;
          el.eachChild([&](const Block& child) {
            if (broadcastEvent(child, el.childBox(i++, box, e), ev)) consumed = true;
          });
        }
      },
      b.v
  );
  return consumed;
}

namespace {

struct FocusedBox {
  const Block* block;
  Box box;
};

void collectFocus(const Block& b, const Box& box, const Engine& e, std::vector<FocusedBox>& out) {
  if (const auto* c = b.get<Custom>()) {
    bool transparent = c->childCount() > 0;
    if (c->handles()) out.push_back({&b, box});
    if (transparent) {
      c->visit(box, e, [&](const Block& child, Box cb) { collectFocus(child, cb, e, out); });
    }
    return;
  }
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_handle<T>::value) out.push_back({&b, box});
        if constexpr (has_children<T>::value) {
          std::size_t i = 0;
          el.eachChild([&](const Block& child) {
            collectFocus(child, el.childBox(i++, box, e), e, out);
          });
        }
      },
      b.v
  );
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

int deliverKey(const Block& root, const Box& box, int current, Key key) {
  Engine e;
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  if (found.empty()) return 0;
  int n = static_cast<int>(found.size());
  int i = current < 0 ? 0 : (current >= n ? n - 1 : current);
  if (key == Key::Activate) {
    handle(*found[static_cast<std::size_t>(i)].block, Event::borrow(Activate{}));
    return i;
  }
  if (handle(*found[static_cast<std::size_t>(i)].block, Event::borrow(KeyPress{key}))) return i;
  switch (key) {
    case Key::Left:
      return moveFocus(root, box, i, FocusDir::Left);
    case Key::Right:
      return moveFocus(root, box, i, FocusDir::Right);
    case Key::Up:
      return moveFocus(root, box, i, FocusDir::Up);
    case Key::Down:
      return moveFocus(root, box, i, FocusDir::Down);
    case Key::Next:
      return moveFocus(root, box, i, FocusDir::Next);
    case Key::Prev:
      return moveFocus(root, box, i, FocusDir::Prev);
    default:
      return i;
  }
}

Box focusRingBox(const Block& root, const Box& box, int index, float pad) {
  Engine e;
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  if (index < 0 || static_cast<std::size_t>(index) >= found.size()) return {0, 0, 0, 0};
  Box b = found[static_cast<std::size_t>(index)].box;
  return {b.x - pad, b.y - pad, b.w + 2 * pad, b.h + 2 * pad};
}

void recordFocusRing(Rec& rec, const Block& root, const Box& box, int index, float pad) {
  Engine e;
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  if (index < 0 || static_cast<std::size_t>(index) >= found.size()) return;
  rec.push(kFocusRingKind, focusRingBox(root, box, index, pad));
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
      case FocusDir::Right:
        beyond = dx > 0.5f;
        primary = dx;
        secondary = std::fabs(dy);
        break;
      case FocusDir::Left:
        beyond = dx < -0.5f;
        primary = -dx;
        secondary = std::fabs(dy);
        break;
      case FocusDir::Down:
        beyond = dy > 0.5f;
        primary = dy;
        secondary = std::fabs(dx);
        break;
      case FocusDir::Up:
        beyond = dy < -0.5f;
        primary = -dy;
        secondary = std::fabs(dx);
        break;
      default:
        break;
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
