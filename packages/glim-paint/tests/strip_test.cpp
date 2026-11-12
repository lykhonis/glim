// Row-banded solid polyline and rounded-stroke flattens match a
// brute-force per-pixel scan.

#include "Strip.h"
#include "StripRows.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using glim::Vec2;
using glim::paint::Strip;

float coverageAt(const std::vector<Strip>& strips, int x, int y) {
  for (const Strip& s : strips) {
    if (static_cast<float>(y) < s.y || static_cast<float>(y) >= s.y + s.height) continue;
    for (const auto& sp : s.spans)
      if (static_cast<float>(x) >= sp.x0 && static_cast<float>(x) < sp.x1) return sp.coverage;
  }
  return 0.f;
}

std::uint32_t lcg(std::uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return state >> 8;
}

}  // namespace

int main() {
  std::uint32_t seed = 7;
  int failures = 0;
  for (int c = 0; c < 24; ++c) {
    const std::size_t n = 2 + lcg(seed) % 7;
    std::vector<Vec2> pts;
    for (std::size_t i = 0; i < n; ++i)
      pts.push_back(
          {static_cast<float>(lcg(seed) % 2000) / 10.f, static_cast<float>(lcg(seed) % 1200) / 10.f}
      );
    const float width = 0.6f + static_cast<float>(lcg(seed) % 40) / 10.f;
    std::vector<Strip> strips;
    glim::paint::flattenSolidPolyline(pts.data(), pts.size(), width, strips);
    for (int y = -8; y < 132; ++y)
      for (int x = -8; x < 212; ++x) {
        float want = glim::paint::polylineCoverage(
            pts.data(),
            pts.size(),
            width,
            0.f,
            static_cast<float>(x) + 0.5f,
            static_cast<float>(y) + 0.5f
        );
        const float got = coverageAt(strips, x, y);
        if (got != glim::paint::snapCoverage(want) && ++failures < 5)
          std::cerr << "case " << c << " px " << x << "," << y << " got " << got << " want " << want
                    << "\n";
      }
  }
  // Rounded strokes: banded rows skip the hole, the coverage stays the
  // outer-minus-inner difference everywhere.
  for (int c = 0; c < 40; ++c) {
    using glim::Radius;
    using glim::Rect;
    const Rect rect{
        {static_cast<float>(lcg(seed) % 400) / 10.f, static_cast<float>(lcg(seed) % 400) / 10.f},
        {static_cast<float>(lcg(seed) % 1600) / 10.f, static_cast<float>(lcg(seed) % 900) / 10.f}
    };
    auto pick = [&] { return static_cast<float>(lcg(seed) % 300) / 10.f; };
    const Radius radius = c % 2 ? Radius{pick()} : Radius{pick(), pick(), pick(), pick()};
    const float width = 0.5f + static_cast<float>(lcg(seed) % 40) / 10.f;
    std::vector<Strip> strips;
    glim::paint::flattenStroke(rect, radius, width, strips);
    const float h = width * 0.5f;
    const Rect outer{
        {rect.origin.x - h, rect.origin.y - h}, {rect.size.x + width, rect.size.y + width}
    };
    const Rect inner{
        {rect.origin.x + h, rect.origin.y + h}, {rect.size.x - width, rect.size.y - width}
    };
    const Radius orad{radius.lt + h, radius.rt + h, radius.lb + h, radius.rb + h};
    const Radius irad{
        std::max(0.f, radius.lt - h),
        std::max(0.f, radius.rt - h),
        std::max(0.f, radius.lb - h),
        std::max(0.f, radius.rb - h)
    };
    for (int y = -8; y < 152; ++y)
      for (int x = -8; x < 212; ++x) {
        const float px = static_cast<float>(x) + 0.5f, py = static_cast<float>(y) + 0.5f;
        float want = glim::paint::roundedCoverage(outer, orad, px, py);
        if (inner.size.x > 1e-4f && inner.size.y > 1e-4f)
          want = std::max(0.f, want - glim::paint::roundedCoverage(inner, irad, px, py));
        const float got = coverageAt(strips, x, y);
        if (got != glim::paint::snapCoverage(want) && ++failures < 5)
          std::cerr << "stroke case " << c << " px " << x << "," << y << " got " << got << " want "
                    << want << "\n";
      }
  }
  if (failures) {
    std::cerr << failures << " pixel mismatches\n";
    return EXIT_FAILURE;
  }
  std::cout << "strip_test ok\n";
  return EXIT_SUCCESS;
}
