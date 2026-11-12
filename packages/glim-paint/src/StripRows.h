#pragma once

#include <vector>

#include "Strip.h"

namespace glim::paint {

inline constexpr float kEps = 1e-4f;

inline bool sharp(const Radius& r) {
  return r.lt <= kEps && r.rt <= kEps && r.lb <= kEps && r.rb <= kEps;
}

inline float snapCoverage(float c) {
  if (c >= 1.f - 1e-3f) return 1.f;
  if (c <= 1e-4f) return 0.f;
  return c;
}

inline bool sameCoverage(float a, float b) { return std::fabs(a - b) <= 1e-3f; }

inline void addSpan(Strip& strip, float x0, float x1, float coverage) {
  if (x1 <= x0 + 1e-6f || coverage <= 1e-4f) {
    return;
  }
  if (!strip.spans.empty()) {
    StripSpan& last = strip.spans.back();
    if (sameCoverage(last.coverage, coverage) && x0 <= last.x1 + 1e-3f) {
      last.x1 = std::max(last.x1, x1);
      return;
    }
  }
  strip.spans.push_back({x0, x1, coverage});
}

inline void packHeight1(std::vector<Strip>& strips) {
  if (strips.size() < 2) {
    return;
  }
  std::vector<Strip> packed;
  packed.reserve(strips.size());
  std::size_t i = 0;
  while (i < strips.size()) {
    Strip cur = std::move(strips[i]);
    ++i;
    while (i < strips.size() && std::fabs(cur.height - 1.f) <= kEps &&
           std::fabs(strips[i].height - 1.f) <= kEps &&
           std::fabs(strips[i].y - (cur.y + cur.height)) <= kEps &&
           cur.spans.size() == strips[i].spans.size() &&
           cur.height + 1.f <= static_cast<float>(kStripHeight) + kEps) {
      bool same = true;
      for (std::size_t s = 0; s < cur.spans.size(); ++s) {
        if (std::fabs(cur.spans[s].x0 - strips[i].spans[s].x0) > 1e-3f ||
            std::fabs(cur.spans[s].x1 - strips[i].spans[s].x1) > 1e-3f ||
            !sameCoverage(cur.spans[s].coverage, strips[i].spans[s].coverage)) {
          same = false;
          break;
        }
      }
      if (!same) {
        break;
      }
      cur.height += 1.f;
      ++i;
    }
    packed.push_back(std::move(cur));
  }
  strips = std::move(packed);
}

// Pixel columns [a, b) of one row that may carry coverage.
struct ColumnRange {
  int a, b;
};

// Coverage sampled per row over `rows(y, ranges)` windows (at most two);
// pixels outside the windows must have zero coverage.
template <typename Rows, typename Cov>
void flattenCoverageRows(
    int x0, int y0, int x1, int y1, std::vector<Strip>& out, Rows&& rows, Cov&& covAt
) {
  if (y1 > y0) out.reserve(out.size() + static_cast<std::size_t>(y1 - y0));
  for (int y = y0; y < y1; ++y) {
    Strip s;
    s.y = static_cast<float>(y);
    s.height = 1.f;
    s.spans.reserve(4);
    ColumnRange ranges[2];
    const int n = rows(y, ranges);
    for (int k = 0; k < n; ++k) {
      const int ra = std::max(x0, ranges[k].a), rb = std::min(x1, ranges[k].b);
      int runX = ra;
      float runCov = -1.f;
      auto flush = [&](int x) {
        if (runCov > 1e-4f) {
          addSpan(s, static_cast<float>(runX), static_cast<float>(x), runCov);
        }
        runCov = -1.f;
      };
      for (int x = ra; x < rb; ++x) {
        const float cov =
            snapCoverage(covAt(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f));
        if (runCov < 0.f) {
          runX = x;
          runCov = cov;
        } else if (sameCoverage(cov, runCov)) {
          continue;
        } else {
          flush(x);
          runX = x;
          runCov = cov;
        }
      }
      flush(rb);
    }
    if (!s.spans.empty()) {
      out.push_back(std::move(s));
    }
  }
  packHeight1(out);
}

template <typename Cov>
void flattenCoverage(int x0, int y0, int x1, int y1, std::vector<Strip>& out, Cov&& covAt) {
  flattenCoverageRows(
      x0,
      y0,
      x1,
      y1,
      out,
      [&](int, ColumnRange* r) {
        r[0] = {x0, x1};
        return 1;
      },
      covAt
  );
}

}  // namespace glim::paint
