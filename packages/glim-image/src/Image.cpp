#include <glim/image/Image.h>

#include <cstdlib>

namespace glim::image {

Diff compare(const Image& a, const Image& b, int maxChannelDelta) {
  Diff diff;
  if (a.empty() || b.empty() || a.width != b.width || a.height != b.height ||
      a.rgba.size() != b.rgba.size()) {
    return diff;
  }
  diff.sameSize = true;
  const int maxD = maxChannelDelta < 0 ? 0 : maxChannelDelta;
  for (std::size_t i = 0; i < a.rgba.size(); i += 4) {
    int worst = 0;
    for (std::size_t c = 0; c < 4; ++c) {
      const int d = std::abs(static_cast<int>(a.rgba[i + c]) - static_cast<int>(b.rgba[i + c]));
      if (d > maxD) {
        ++diff.overDelta;
      }
      if (c < 3 && d > worst) {
        worst = d;
      }
    }
    if (worst > maxD) {
      ++diff.overPixels;
      if (worst > diff.maxDelta) {
        diff.maxDelta = worst;
      }
    }
  }
  return diff;
}

}  // namespace glim::image
