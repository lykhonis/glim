#define STB_TRUETYPE_IMPLEMENTATION
#include "Font.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

#include "Parallel.h"
#include "inter_latin.h"
#include "stb_truetype.h"

namespace glim::paint {
namespace {

constexpr int kFirstGlyph = 32;
constexpr int kEndGlyph = 127;

struct GlyphSdf {
  unsigned char* pixels = nullptr;
  int w = 0, h = 0, xoff = 0, yoff = 0;
};

// Glyph SDFs are independent and dominate the bake (~20 ms serial), so they
// are rendered across cores; packing stays serial, so the atlas is unchanged.
std::vector<GlyphSdf> renderSdfs(const stbtt_fontinfo& info, float scale) {
  const float pixelDistScale = static_cast<float>(kSdfOnEdge) / static_cast<float>(kSdfPad);
  std::vector<GlyphSdf> sdfs(kEndGlyph - kFirstGlyph);
  parallelFor(sdfs.size(), [&](std::size_t i) {
    GlyphSdf& g = sdfs[i];
    g.pixels = stbtt_GetCodepointSDF(
        &info,
        scale,
        kFirstGlyph + static_cast<int>(i),
        kSdfPad,
        kSdfOnEdge,
        pixelDistScale,
        &g.w,
        &g.h,
        &g.xoff,
        &g.yoff
    );
  });
  return sdfs;
}

FontAtlas bakeLatin() {
  FontAtlas atlas;
  stbtt_fontinfo info{};
  if (kInterLatinTtfSize < 4 ||
      !stbtt_InitFont(&info, kInterLatinTtf, stbtt_GetFontOffsetForIndex(kInterLatinTtf, 0))) {
    return atlas;
  }

  const float scale = stbtt_ScaleForPixelHeight(&info, kSdfBakePx);
  int ascent = 0;
  int descent = 0;
  int lineGap = 0;
  stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
  atlas.ascent = static_cast<float>(ascent) * scale;
  atlas.descent = static_cast<float>(descent) * scale;
  atlas.lineHeight = static_cast<float>(ascent - descent + lineGap) * scale;
  atlas.bakePx = kSdfBakePx;

  constexpr int kAtlas = 512;
  atlas.width = kAtlas;
  atlas.height = kAtlas;
  atlas.rgba.assign(static_cast<std::size_t>(kAtlas * kAtlas * 4), 0);

  std::vector<GlyphSdf> sdfs = renderSdfs(info, scale);
  int x = 1;
  int y = 1;
  int rowH = 0;
  for (int c = kFirstGlyph; c < kEndGlyph; ++c) {
    FontGlyph& g = atlas.glyphs[c];
    int adv = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(&info, c, &adv, &lsb);
    g.advance = static_cast<float>(adv) * scale;
    g.present = true;

    const GlyphSdf& rendered = sdfs[static_cast<std::size_t>(c - kFirstGlyph)];
    unsigned char* sdf = rendered.pixels;
    const int gw = rendered.w, gh = rendered.h, xoff = rendered.xoff, yoff = rendered.yoff;
    if (!sdf || gw <= 0 || gh <= 0) {
      if (sdf) {
        stbtt_FreeSDF(sdf, nullptr);
      }
      continue;
    }
    if (x + gw + 1 >= kAtlas) {
      x = 1;
      y += rowH + 1;
      rowH = 0;
    }
    if (y + gh + 1 >= kAtlas) {
      stbtt_FreeSDF(sdf, nullptr);
      continue;
    }
    for (int row = 0; row < gh; ++row) {
      for (int col = 0; col < gw; ++col) {
        const unsigned char d = sdf[row * gw + col];
        const std::size_t i = static_cast<std::size_t>(((y + row) * kAtlas + (x + col)) * 4);
        atlas.rgba[i + 0] = d;
        atlas.rgba[i + 1] = d;
        atlas.rgba[i + 2] = d;
        atlas.rgba[i + 3] = 255;
      }
    }
    g.xoff = static_cast<float>(xoff);
    g.yoff = static_cast<float>(yoff);
    g.w = static_cast<float>(gw);
    g.h = static_cast<float>(gh);
    g.uv = {
        {static_cast<float>(x) / static_cast<float>(kAtlas),
         static_cast<float>(y) / static_cast<float>(kAtlas)},
        {static_cast<float>(gw) / static_cast<float>(kAtlas),
         static_cast<float>(gh) / static_cast<float>(kAtlas)}
    };
    x += gw + 1;
    rowH = std::max(rowH, gh);
    stbtt_FreeSDF(sdf, nullptr);
  }
  return atlas;
}

}  // namespace

const FontAtlas& latinAtlas() {
  static std::once_flag once;
  static FontAtlas atlas;
  std::call_once(once, [] { atlas = bakeLatin(); });
  return atlas;
}

}  // namespace glim::paint
