#pragma once

// Pixel analysis for matching a render against a reference: scores,
// visual diff sheets, and geometry probes. Pure functions on Images.

#include <glim/image/Image.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace glim::image {

struct Region {
  int x = 0, y = 0, w = 0, h = 0;
};

struct Rgb {
  std::uint8_t r = 0, g = 0, b = 0;
};

// Row (`row` true) or column line in image coordinates, inclusive range.
struct Line {
  bool row = true;
  int at = 0, from = 0, to = -1;
};

Rgb pixel(const Image&, int x, int y);
int luma(Rgb);

Image crop(const Image&, Region);
Image scaleNearest(const Image&, int factor);
Image blend(const Image& a, const Image& b, float t);
// |a-b| amplified by gain, orange-on-black; same size inputs.
Image heat(const Image& a, const Image& b, int gain = 4);
// Concatenates images with magenta gaps; shorter ones are top/left aligned.
Image stack(const std::vector<Image>& parts, bool vertical, int gap = 4);
// Grid every `minor` source px over an image scaled by `scale` from `origin`;
// lines and labels at multiples of 50 are bright.
void drawRuler(Image& scaled, Region origin, int scale, int minor = 10);

// Mean absolute RGB difference, 0..255. Different sizes score 255.
double mad(const Image& a, const Image& b);
// MAD of blurred edge maps: sensitive to geometry, not to fill tone.
double edgeMad(const Image& a, const Image& b);

struct Mask {
  enum class Kind { Orange, Bright, Dim, Near } kind = Kind::Bright;
  Rgb color{};
  int tolerance = 24;
  bool test(Rgb) const;
};
// "orange" | "bright" | "dim" | "#rrggbb[:tol]".
bool parseMask(std::string_view, Mask*);

struct Bounds {
  int x0 = 0, y0 = 0, x1 = -1, y1 = -1;
  long count = 0;
  double cx = 0, cy = 0;
};
Bounds maskBounds(const Image&, Region, const Mask&);

struct Run {
  int from = 0, to = 0;
};
std::vector<Run> maskRuns(const Image&, Line, const Mask&);

struct Peak {
  int at = 0, luma = 0;
};
// Local luma maxima >= minLuma (plateaus report their first pixel).
std::vector<Peak> lumaPeaks(const Image&, Line, int minLuma);
std::vector<int> lumaLine(const Image&, Line);
// Luma peaks along a ray from (cx, cy) at `deg` (clockwise from +x, y down);
// Peak::at is the radius.
std::vector<Peak> rayPeaks(
    const Image&, float cx, float cy, float deg, int r0, int r1, int minLuma
);

struct ArcRun {
  float fromDeg = 0, toDeg = 0;
};
// Mask runs along a circle, sampled every `stepDeg` from deg0 to deg1.
std::vector<ArcRun> ringRuns(
    const Image&,
    float cx,
    float cy,
    float r,
    const Mask&,
    float deg0 = 0,
    float deg1 = 360,
    float stepDeg = 0.25f
);

struct InkStats {
  Rgb ink, background;
};
// Median of the brightest `inkFraction` pixels and the 25th luma percentile.
InkStats inkStats(const Image&, Region, double inkFraction);

}  // namespace glim::image
