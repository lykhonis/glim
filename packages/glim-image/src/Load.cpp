#include <glim/image/Load.h>
#include <glim/image/Png.h>

#include <cstdio>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <algorithm>
#include <vector>

#include "stb_image.h"

namespace glim::image {

bool load(const char* path, Image* out) {
  if (!path || !out) return false;
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  std::vector<unsigned char> bytes;
  unsigned char buf[65536];
  std::size_t n = 0;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) bytes.insert(bytes.end(), buf, buf + n);
  std::fclose(f);
  static constexpr unsigned char kPngSig[8] = {137, 'P', 'N', 'G', '\r', '\n', 26, '\n'};
  // Png::read covers glim's own RGBA output; other PNG layouts (RGB
  // browser screenshots) fall through to stb.
  if (bytes.size() >= 8 && std::equal(kPngSig, kPngSig + 8, bytes.begin()) && Png::read(path, out))
    return true;
  int w = 0, h = 0, comp = 0;
  unsigned char* px =
    stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &comp, 4);
  if (!px) return false;
  *out = Image::rgba8(w, h);
  std::copy(px, px + out->byteCount(), out->rgba.begin());
  stbi_image_free(px);
  return true;
}

}  // namespace glim::image
