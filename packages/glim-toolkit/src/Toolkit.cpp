#include "glim/toolkit/Toolkit.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <sstream>
#include <type_traits>
#include <variant>
#include <vector>

#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

namespace detail {

float clampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

Box measureText(const std::string& text, Constraints c, const Engine& e) {
  float w = static_cast<float>(text.size()) * Tokens::kTypeAdvance;
  if (const Measurer* m = e.get<Measurer>()) {
    if (m->measure) w = m->measure(text.c_str(), Tokens::kTypeLine);
  }
  return resolveSize(w, Tokens::kTypeLine, c);
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
  walk(
      b,
      box,
      *this,
      [&](const Block& n, const Box& bx) -> bool {
        describeSelf(n, bx, r);
        return true;
      },
      [](const Block&, const Box&) {}
  );
}

Box place(const Block& b, Constraints c) { return Engine{}.place(b, c); }

void record(const Block& b, const Box& box, Rec& r, const Engine& e) {
  Engine ee = e;
  ee.record(b, box, r);
}

namespace {

bool contains(const Box& b, float x, float y) {
  return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

}  // namespace

const Block* hit(const Block& b, const Box& box, float x, float y, const Engine& e) {
  const Block* found = nullptr;
  int matches = 0;
  struct Claim {
    const Block* node;
    int entered;
  };
  std::vector<Claim> claims;
  walk(
      b,
      box,
      e,
      [&](const Block& n, const Box& bx) -> bool {
        if (claimsHit(n)) {
          if (contains(bx, x, y)) {
            found = &n;
            ++matches;
            return false;  // claimed: whole region resolves here
          }
          claims.push_back({&n, matches});
          return true;  // missed chrome: a spilled child may still hit
        }
        if (contains(bx, x, y)) {
          found = &n;
          ++matches;
        }
        return true;
      },
      [&](const Block& n, const Box&) {
        if (!claims.empty() && claims.back().node == &n) {
          if (matches > claims.back().entered) found = &n;
          claims.pop_back();
        }
      }
  );
  return found;
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
      [&](const Block& n, const Box& bx) -> bool {
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
      [&](const Block&, const Box&) { --depth; }
  );
}

void dump(std::ostream& os, const Block& b, const Box& box, const Engine& e, int indent) {
  dumpIn(os, b, box, e, indent);
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

bool handleAt(const Block& b, const Box& box, const Event& ev) {
  if (const auto* c = b.get<Custom>()) return c->handleAt(box, ev);
  bool consumed = false;
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_handle_at<T>::value)
          consumed = el.handleAt(box, ev);
        else if constexpr (has_handle<T>::value)
          consumed = el.handle(ev);
      },
      b.v
  );
  return consumed;
}

bool dispatchPointer(
    const Block& b, const Box& box, float x, float y, const Event& ev, const Engine& e
) {
  const Block* t = hit(b, box, x, y, e);
  if (!t) return false;
  // The hit node is known; its laid-out box is found with one more
  // walk (pointer identity — the tree outlives this call).
  Box hitBox{0, 0, 0, 0};
  bool found = false;
  walk(
      b,
      box,
      e,
      [&](const Block& n, const Box& bx) -> bool {
        if (!found && &n == t) {
          hitBox = bx;
          found = true;
        }
        return true;
      },
      [](const Block&, const Box&) {}
  );
  return handleAt(*t, hitBox, ev);
}

bool broadcastEvent(const Block& b, const Box& box, const Event& ev, const Engine& e) {
  bool consumed = false;
  walk(
      b,
      box,
      e,
      [&](const Block& n, const Box&) -> bool {
        if (handle(n, ev)) {
          consumed = true;
          return false;  // handling node prunes its own subtree, not siblings
        }
        return true;
      },
      [](const Block&, const Box&) {}
  );
  return consumed;
}

namespace {

struct FocusedBox {
  const Block* block;
  Box box;
};

void collectFocus(const Block& b, const Box& box, const Engine& e, std::vector<FocusedBox>& out) {
  walk(
      b,
      box,
      e,
      [&](const Block& n, const Box& bx) -> bool {
        if (canHandle(n)) out.push_back({&n, bx});
        return walkHasChildren(n);
      },
      [](const Block&, const Box&) {}
  );
}

}  // namespace

std::vector<const Block*> focusables(const Block& root, const Box& box, const Engine& e) {
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  std::vector<const Block*> out;
  for (const auto& f : found) out.push_back(f.block);
  return out;
}

Box focusBox(const Block& root, const Box& box, int index, const Engine& e) {
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  if (index < 0 || static_cast<std::size_t>(index) >= found.size()) return {0, 0, 0, 0};
  return found[static_cast<std::size_t>(index)].box;
}

int focusAt(const Block& root, const Box& box, float x, float y, const Engine& e) {
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  for (std::size_t i = 0; i < found.size(); ++i) {
    const Box& b = found[i].box;
    if (b.x <= x && x < b.x + b.w && b.y <= y && y < b.y + b.h) return static_cast<int>(i);
  }
  return -1;
}

bool dispatchCaptured(
    const Block& root, const Box& box, int index, const Event& ev, const Engine& e
) {
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  if (index < 0 || static_cast<std::size_t>(index) >= found.size()) return false;
  const FocusedBox& f = found[static_cast<std::size_t>(index)];
  // Unlike dispatchPointer, the return reports routing, not
  // consumption: a routed-but-ignored move (slider value unchanged)
  // must NOT read as stale, or slow drags would drop the capture.
  handleAt(*f.block, f.box, ev);
  return true;
}

int deliverKey(const Block& root, const Box& box, int current, Key key, const Engine& e) {
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  if (found.empty()) return 0;
  int n = static_cast<int>(found.size());
  // Focus-visible entry: Tab from "nothing" lands on the first item
  // (Shift+Tab on the last), not one past it.
  if (current < 0) {
    if (key == Key::Prev) return n - 1;
    if (key == Key::Next || key == Key::Down || key == Key::Right || key == Key::Up ||
        key == Key::Left)
      return 0;
  }
  int i = current < 0 ? 0 : (current >= n ? n - 1 : current);
  if (key == Key::Activate) {
    handle(*found[static_cast<std::size_t>(i)].block, Event::borrow(Activate{}));
    return i;
  }
  if (handle(*found[static_cast<std::size_t>(i)].block, Event::borrow(KeyPress{key}))) return i;
  switch (key) {
    case Key::Left:
      return moveFocus(root, box, i, FocusDir::Left, e);
    case Key::Right:
      return moveFocus(root, box, i, FocusDir::Right, e);
    case Key::Up:
      return moveFocus(root, box, i, FocusDir::Up, e);
    case Key::Down:
      return moveFocus(root, box, i, FocusDir::Down, e);
    case Key::Next:
      return moveFocus(root, box, i, FocusDir::Next, e);
    case Key::Prev:
      return moveFocus(root, box, i, FocusDir::Prev, e);
    default:
      return i;
  }
}

Box focusRingBox(const Block& root, const Box& box, int index, float pad, const Engine& e) {
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  if (index < 0 || static_cast<std::size_t>(index) >= found.size()) return {0, 0, 0, 0};
  Box b = found[static_cast<std::size_t>(index)].box;
  return {b.x - pad, b.y - pad, b.w + 2 * pad, b.h + 2 * pad};
}

void recordFocusRing(
    Rec& rec, const Block& root, const Box& box, int index, float pad, const Engine& e
) {
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  if (index < 0 || static_cast<std::size_t>(index) >= found.size()) return;
  rec.push(kFocusRingKind, focusRingBox(root, box, index, pad, e));
}

int moveFocus(const Block& root, const Box& box, int current, FocusDir dir, const Engine& e) {
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
