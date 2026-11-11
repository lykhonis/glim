#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace glim::image {

struct Image {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba;

  static Image rgba8(int width, int height) {
    Image im;
    if (width > 0 && height > 0) {
      im.width = width;
      im.height = height;
      im.rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4, 0);
    }
    return im;
  }

  std::size_t byteCount() const {
    if (width <= 0 || height <= 0) {
      return 0;
    }
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
  }

  bool empty() const { return byteCount() == 0 || rgba.size() != byteCount(); }
};

struct Diff {
  bool sameSize = false;
  std::size_t overDelta = 0;   // channels beyond tolerance (RGBA)
  std::size_t overPixels = 0;  // pixels with an RGB channel beyond tolerance
  int maxDelta = 0;            // largest RGB channel delta among those pixels

  bool match() const { return sameSize && overDelta == 0; }
};

Diff compare(const Image& a, const Image& b, int maxChannelDelta = 2);

}  // namespace glim::image
