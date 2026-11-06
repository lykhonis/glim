#include <cmath>
#include <utility>
#include <vector>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/Toolkit.h"

namespace glim {
namespace toolkit {

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
      [&](const Block& n, const Box& bx, const Engine&) -> bool {
        if (canHandle(n)) out.push_back({&n, bx});
        return walkHasChildren(n);
      },
      [](const Block&, const Box&, const Engine&) {}
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
  // Return is routing, not consumption: routed-but-ignored moves must not read as stale.
  handleAt(*f.block, f.box, ev);
  return true;
}

int deliverKey(const Block& root, const Box& box, int current, Key key, const Engine& e) {
  std::vector<FocusedBox> found;
  collectFocus(root, box, e, found);
  if (found.empty()) return 0;
  int n = static_cast<int>(found.size());
  // Tab from nothing lands on first item (Shift+Tab on last).
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
