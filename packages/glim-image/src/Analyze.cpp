#include <glim/image/Analyze.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace glim::image {
namespace {

std::size_t at(const Image& im, int x, int y) {
  return (static_cast<std::size_t>(y) * static_cast<std::size_t>(im.width) +
          static_cast<std::size_t>(x)) *
         4;
}

void put(Image& im, int x, int y, Rgb c) {
  if (x < 0 || y < 0 || x >= im.width || y >= im.height) return;
  const std::size_t i = at(im, x, y);
  im.rgba[i] = c.r;
  im.rgba[i + 1] = c.g;
  im.rgba[i + 2] = c.b;
  im.rgba[i + 3] = 255;
}

Region clampRegion(const Image& im, Region r) {
  const int x0 = std::clamp(r.x, 0, im.width), y0 = std::clamp(r.y, 0, im.height);
  const int x1 = std::clamp(r.x + r.w, 0, im.width), y1 = std::clamp(r.y + r.h, 0, im.height);
  return {x0, y0, x1 - x0, y1 - y0};
}

// Laplacian magnitude, then two 3x3 box passes (~gaussian sigma 1.15).
std::vector<int> edgeMap(const Image& im) {
  const int w = im.width, h = im.height;
  std::vector<int> l(static_cast<std::size_t>(w * h)), e(l.size(), 0), t(l.size(), 0);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) l[y * w + x] = luma(pixel(im, x, y));
  for (int y = 1; y + 1 < h; ++y)
    for (int x = 1; x + 1 < w; ++x) {
      int s = 0;
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) s += l[(y + dy) * w + x + dx];
      e[y * w + x] = std::min(255, std::abs(9 * l[y * w + x] - s));
    }
  for (int pass = 0; pass < 2; ++pass) {
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        int s = 0, n = 0;
        for (int dx = -1; dx <= 1; ++dx)
          if (x + dx >= 0 && x + dx < w) s += e[y * w + x + dx], ++n;
        t[y * w + x] = s / n;
      }
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        int s = 0, n = 0;
        for (int dy = -1; dy <= 1; ++dy)
          if (y + dy >= 0 && y + dy < h) s += t[(y + dy) * w + x], ++n;
        e[y * w + x] = s / n;
      }
  }
  return e;
}

// 3x5 digit glyphs, rows top to bottom, 3 bits each (MSB = left).
constexpr std::uint8_t kDigits[10][5] = {
  {7, 5, 5, 5, 7},
  {2, 6, 2, 2, 7},
  {7, 1, 7, 4, 7},
  {7, 1, 7, 1, 7},
  {5, 5, 7, 1, 1},
  {7, 4, 7, 1, 7},
  {7, 4, 7, 5, 7},
  {7, 1, 1, 1, 1},
  {7, 5, 7, 5, 7},
  {7, 5, 7, 1, 7},
};

void drawNumber(Image& im, int x, int y, int value, Rgb c) {
  const std::string s = std::to_string(value);
  for (char ch : s) {
    const auto& g = kDigits[ch - '0'];
    for (int r = 0; r < 5; ++r)
      for (int b = 0; b < 3; ++b)
        if (g[r] & (4 >> b))
          for (int k = 0; k < 4; ++k) put(im, x + b * 2 + (k & 1), y + r * 2 + (k >> 1), c);
    x += 8;
  }
}

}  // namespace

Rgb pixel(const Image& im, int x, int y) {
  const std::size_t i = at(im, x, y);
  return {im.rgba[i], im.rgba[i + 1], im.rgba[i + 2]};
}

int luma(Rgb c) { return (c.r * 299 + c.g * 587 + c.b * 114 + 500) / 1000; }

Image crop(const Image& im, Region r) {
  r = clampRegion(im, r);
  Image out = Image::rgba8(r.w, r.h);
  for (int y = 0; y < r.h; ++y)
    std::copy_n(
      &im.rgba[at(im, r.x, r.y + y)], static_cast<std::size_t>(r.w) * 4, &out.rgba[at(out, 0, y)]
    );
  return out;
}

Image scaleNearest(const Image& im, int f) {
  f = std::max(1, f);
  Image out = Image::rgba8(im.width * f, im.height * f);
  for (int y = 0; y < out.height; ++y)
    for (int x = 0; x < out.width; ++x)
      std::copy_n(&im.rgba[at(im, x / f, y / f)], 4, &out.rgba[at(out, x, y)]);
  return out;
}

Image blend(const Image& a, const Image& b, float t) {
  Image out = a;
  if (a.width != b.width || a.height != b.height) return out;
  for (std::size_t i = 0; i < out.rgba.size(); ++i)
    out.rgba[i] = static_cast<std::uint8_t>(std::lround(a.rgba[i] * (1.f - t) + b.rgba[i] * t));
  return out;
}

Image heat(const Image& a, const Image& b, int gain) {
  Image out = Image::rgba8(a.width, a.height);
  if (a.width != b.width || a.height != b.height) return out;
  for (int y = 0; y < a.height; ++y)
    for (int x = 0; x < a.width; ++x) {
      const Rgb p = pixel(a, x, y), q = pixel(b, x, y);
      const int d = (std::abs(p.r - q.r) + std::abs(p.g - q.g) + std::abs(p.b - q.b)) / 3;
      const int v = std::min(255, d * gain);
      put(out, x, y, {static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v / 3), 0});
    }
  return out;
}

Image stack(const std::vector<Image>& parts, bool vertical, int gap) {
  int w = 0, h = 0;
  for (const Image& p : parts) {
    if (vertical)
      w = std::max(w, p.width), h += p.height + gap;
    else
      h = std::max(h, p.height), w += p.width + gap;
  }
  (vertical ? h : w) -= parts.empty() ? 0 : gap;
  Image out = Image::rgba8(std::max(1, w), std::max(1, h));
  for (int y = 0; y < out.height; ++y)
    for (int x = 0; x < out.width; ++x) put(out, x, y, {255, 0, 255});
  int off = 0;
  for (const Image& p : parts) {
    for (int y = 0; y < p.height; ++y)
      std::copy_n(
        &p.rgba[at(p, 0, y)],
        static_cast<std::size_t>(p.width) * 4,
        &out.rgba[vertical ? at(out, 0, off + y) : at(out, off, y)]
      );
    off += (vertical ? p.height : p.width) + gap;
  }
  return out;
}

void drawRuler(Image& im, Region o, int s, int minor) {
  minor = std::max(1, minor);
  const Rgb dim{0, 55, 100}, major{0, 140, 255}, label{255, 230, 0};
  for (int gx = (o.x / minor + 1) * minor; gx < o.x + o.w; gx += minor) {
    const bool big = gx % 50 == 0;
    for (int y = 0; y < im.height; ++y) put(im, (gx - o.x) * s, y, big ? major : dim);
    if (big) drawNumber(im, (gx - o.x) * s + 3, 2, gx, label);
  }
  for (int gy = (o.y / minor + 1) * minor; gy < o.y + o.h; gy += minor) {
    const bool big = gy % 50 == 0;
    for (int x = 0; x < im.width; ++x) put(im, x, (gy - o.y) * s, big ? major : dim);
    if (big) drawNumber(im, 2, (gy - o.y) * s + 3, gy, label);
  }
}

double mad(const Image& a, const Image& b) {
  if (a.empty() || a.width != b.width || a.height != b.height) return 255.0;
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < a.rgba.size(); i += 4)
    for (int c = 0; c < 3; ++c)
      sum += static_cast<std::uint64_t>(std::abs(a.rgba[i + c] - b.rgba[i + c]));
  return static_cast<double>(sum) / (static_cast<double>(a.width) * a.height * 3.0);
}

double edgeMad(const Image& a, const Image& b) {
  if (a.empty() || a.width != b.width || a.height != b.height) return 255.0;
  const std::vector<int> ea = edgeMap(a), eb = edgeMap(b);
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < ea.size(); ++i)
    sum += static_cast<std::uint64_t>(std::abs(ea[i] - eb[i]));
  return static_cast<double>(sum) / static_cast<double>(ea.size());
}

bool Mask::test(Rgb c) const {
  switch (kind) {
    case Kind::Orange:
      return c.r > 150 && c.r > c.g + 50 && c.g > c.b && c.b < 90;
    case Kind::Bright:
      return c.r + c.g + c.b > 420;
    case Kind::Dim:
      return c.r + c.g + c.b > 90 && c.r + c.g + c.b <= 420;
    case Kind::Near:
      return std::abs(c.r - color.r) <= tolerance && std::abs(c.g - color.g) <= tolerance &&
             std::abs(c.b - color.b) <= tolerance;
  }
  return false;
}

bool parseMask(std::string_view s, Mask* out) {
  if (s == "orange") return out->kind = Mask::Kind::Orange, true;
  if (s == "bright") return out->kind = Mask::Kind::Bright, true;
  if (s == "dim") return out->kind = Mask::Kind::Dim, true;
  if (s.size() < 7 || s[0] != '#') return false;
  const std::string hex(s.substr(1, 6));
  char* end = nullptr;
  const unsigned long v = std::strtoul(hex.c_str(), &end, 16);
  if (end != hex.c_str() + 6) return false;
  out->kind = Mask::Kind::Near;
  out->color = {
    static_cast<std::uint8_t>(v >> 16),
    static_cast<std::uint8_t>(v >> 8),
    static_cast<std::uint8_t>(v)
  };
  if (s.size() > 8 && s[7] == ':') out->tolerance = std::atoi(std::string(s.substr(8)).c_str());
  return true;
}

Bounds maskBounds(const Image& im, Region r, const Mask& m) {
  r = clampRegion(im, r);
  Bounds b{r.x + r.w, r.y + r.h, -1, -1, 0, 0, 0};
  double sx = 0, sy = 0;
  for (int y = r.y; y < r.y + r.h; ++y)
    for (int x = r.x; x < r.x + r.w; ++x)
      if (m.test(pixel(im, x, y))) {
        b.x0 = std::min(b.x0, x), b.y0 = std::min(b.y0, y);
        b.x1 = std::max(b.x1, x), b.y1 = std::max(b.y1, y);
        sx += x, sy += y, ++b.count;
      }
  if (b.count) b.cx = sx / b.count, b.cy = sy / b.count;
  return b;
}

namespace {
template <class F>
void walkLine(const Image& im, Line l, F&& f) {
  const int limit = (l.row ? im.width : im.height) - 1;
  const int to = l.to < 0 ? limit : std::min(l.to, limit);
  for (int i = std::max(0, l.from); i <= to; ++i)
    f(i, l.row ? pixel(im, i, l.at) : pixel(im, l.at, i));
}
}  // namespace

std::vector<Run> maskRuns(const Image& im, Line l, const Mask& m) {
  std::vector<Run> runs;
  bool open = false;
  walkLine(im, l, [&](int i, Rgb c) {
    const bool on = m.test(c);
    if (on && open)
      runs.back().to = i;
    else if (on)
      runs.push_back({i, i});
    open = on;
  });
  return runs;
}

std::vector<int> lumaLine(const Image& im, Line l) {
  std::vector<int> v;
  walkLine(im, l, [&](int, Rgb c) { v.push_back(luma(c)); });
  return v;
}

namespace {
std::vector<Peak> peaksOf(const std::vector<int>& v, int from, int minLuma) {
  std::vector<Peak> out;
  for (std::size_t i = 0; i < v.size(); ++i) {
    const int prev = i ? v[i - 1] : -1, next = i + 1 < v.size() ? v[i + 1] : -1;
    if (v[i] >= minLuma && v[i] > prev && v[i] >= next)
      out.push_back({from + static_cast<int>(i), v[i]});
  }
  return out;
}
}  // namespace

std::vector<Peak> lumaPeaks(const Image& im, Line l, int minLuma) {
  return peaksOf(lumaLine(im, l), std::max(0, l.from), minLuma);
}

std::vector<Peak> rayPeaks(
  const Image& im, float cx, float cy, float deg, int r0, int r1, int minLuma
) {
  const double a = deg * 3.14159265358979 / 180.0;
  std::vector<int> v;
  for (int r = std::max(0, r0); r <= r1; ++r) {
    const int x = static_cast<int>(std::lround(cx + r * std::cos(a)));
    const int y = static_cast<int>(std::lround(cy + r * std::sin(a)));
    if (x < 0 || y < 0 || x >= im.width || y >= im.height) break;
    v.push_back(luma(pixel(im, x, y)));
  }
  return peaksOf(v, std::max(0, r0), minLuma);
}

std::vector<ArcRun> ringRuns(
  const Image& im, float cx, float cy, float r, const Mask& m, float deg0, float deg1, float step
) {
  std::vector<ArcRun> out;
  bool open = false;
  step = std::max(0.01f, step);
  for (float d = deg0; d <= deg1; d += step) {
    const double a = d * 3.14159265358979 / 180.0;
    const int x = static_cast<int>(std::lround(cx + r * std::cos(a)));
    const int y = static_cast<int>(std::lround(cy + r * std::sin(a)));
    const bool on = x >= 0 && y >= 0 && x < im.width && y < im.height && m.test(pixel(im, x, y));
    if (on && open)
      out.back().toDeg = d;
    else if (on)
      out.push_back({d, d});
    open = on;
  }
  return out;
}

InkStats inkStats(const Image& im, Region r, double frac) {
  const Image c = crop(im, r);
  std::vector<Rgb> px;
  px.reserve(static_cast<std::size_t>(c.width) * c.height);
  for (int y = 0; y < c.height; ++y)
    for (int x = 0; x < c.width; ++x) px.push_back(pixel(c, x, y));
  if (px.empty()) return {};
  std::sort(px.begin(), px.end(), [](Rgb a, Rgb b) { return luma(a) < luma(b); });
  const std::size_t n = px.size();
  const std::size_t k = std::max<std::size_t>(1, static_cast<std::size_t>(n * frac));
  return {px[n - 1 - k / 2], px[n / 4]};
}

}  // namespace glim::image
