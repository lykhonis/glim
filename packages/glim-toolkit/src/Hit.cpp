#include <algorithm>
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

Box intersectBox(const Box& a, const Box& b) {
  const float x0 = std::max(a.x, b.x);
  const float y0 = std::max(a.y, b.y);
  const float x1 = std::min(a.x + a.w, b.x + b.w);
  const float y1 = std::min(a.y + a.h, b.y + b.h);
  return {x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0)};
}

}  // namespace detail

const Block* hit(const Block& b, const Box& box, float x, float y, const Engine& e) {
  const Block* found = nullptr;
  int matches = 0;
  struct Claim {
    const Block* node;
    int entered;
  };
  std::vector<Claim> claims;
  // Clip stack parallel to the walk: containers whose children paint
  // inside a clip group (Roll) bound their subtree's hits. Only fully
  // clipped points miss — partially visible controls still hit, and
  // paints through ContextSink push the same groups.
  std::vector<Box> clips;
  std::vector<bool> pushed;
  const auto visible = [&]() { return clips.empty() || detail::boxContains(clips.back(), x, y); };
  walk(
      b,
      box,
      e,
      [&](const Block& n, const Box& bx) -> bool {
        pushed.push_back(clipsChildren(n));
        if (clipsChildren(n)) {
          clips.push_back(clips.empty() ? bx : detail::intersectBox(clips.back(), bx));
        }
        if (claimsHit(n)) {
          if (detail::boxContains(bx, x, y) && visible()) {
            found = &n;
            ++matches;
            return false;  // claimed: whole region resolves here
          }
          claims.push_back({&n, matches});
          return true;  // missed chrome: a spilled child may still hit
        }
        if (detail::boxContains(bx, x, y) && visible()) {
          found = &n;
          ++matches;
        }
        return true;
      },
      [&](const Block& n, const Box&) {
        if (!pushed.empty()) {
          if (pushed.back() && !clips.empty()) clips.pop_back();
          pushed.pop_back();
        }
        if (!claims.empty() && claims.back().node == &n) {
          if (matches > claims.back().entered && visible()) found = &n;
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

}  // namespace toolkit
}  // namespace glim
