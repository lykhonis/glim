#include "RasterPlate.h"

#include <algorithm>
#include <cmath>

namespace glim::paint::raster {

Pixel samplePixel(const std::vector<Pixel>& img, int w, int h, float px, float py) {
  if (w <= 0 || h <= 0 || img.empty()) {
    return Pixel{0, 0, 0, 0};
  }
  const float x = std::clamp(px - 0.5f, 0.f, static_cast<float>(w - 1));
  const float y = std::clamp(py - 0.5f, 0.f, static_cast<float>(h - 1));
  const int x0 = static_cast<int>(x);
  const int y0 = static_cast<int>(y);
  const int x1 = std::min(w - 1, x0 + 1);
  const int y1 = std::min(h - 1, y0 + 1);
  const float fx = x - static_cast<float>(x0);
  const float fy = y - static_cast<float>(y0);
  const Pixel a = img[static_cast<std::size_t>(y0 * w + x0)];
  const Pixel b = img[static_cast<std::size_t>(y0 * w + x1)];
  const Pixel c = img[static_cast<std::size_t>(y1 * w + x0)];
  const Pixel d = img[static_cast<std::size_t>(y1 * w + x1)];
  const float ia = 1.f - fx;
  const float ib = 1.f - fy;
  return Pixel{
      (a.r * ia + b.r * fx) * ib + (c.r * ia + d.r * fx) * fy,
      (a.g * ia + b.g * fx) * ib + (c.g * ia + d.g * fx) * fy,
      (a.b * ia + b.b * fx) * ib + (c.b * ia + d.b * fx) * fy,
      (a.a * ia + b.a * fx) * ib + (c.a * ia + d.a * fx) * fy
  };
}

Pixel sampleDest(const std::vector<Pixel>& dest, int dw, int dh, float sx, float sy) {
  const int ix = std::min(dw - 1, std::max(0, static_cast<int>(sx)));
  const int iy = std::min(dh - 1, std::max(0, static_cast<int>(sy)));
  return dest[static_cast<std::size_t>(iy * dw + ix)];
}

Pixel sampleDestBilinear(const std::vector<Pixel>& dest, int dw, int dh, float sx, float sy) {
  return samplePixel(dest, dw, dh, sx, sy);
}

BackdropPill plateSpacePill(const Rect& r, float radius, Vec2 origin, float sx, float sy) {
  BackdropPill p;
  p.rect.origin.x = (r.origin.x - origin.x) * sx;
  p.rect.origin.y = (r.origin.y - origin.y) * sy;
  p.rect.size.x = r.size.x * sx;
  p.rect.size.y = r.size.y * sy;
  p.radius = radius * std::min(sx, sy);
  return p;
}

PlateSpec specFromGroup(const Group& g, Rect surface, float pr) {
  PlateSpec spec;
  spec.sigma = snapBackdropSigma(g.params.backdropBlur) * pr * 0.5f;
  spec.flat = g.params.backdropFlat;
  BackdropPill pills[kMaxBackdropPills];
  spec.n = collectBackdropPills(g, pills);
  for (int i = 0; i < spec.n; ++i) {
    spec.pills[i] = plateSpacePill(pills[i].rect, pills[i].radius, surface.origin, pr, pr);
  }
  return spec;
}

PlateSpec specFromIsolate(const Isolate& iso, float pr) {
  PlateSpec spec;
  spec.sigma = iso.backdropSigma * pr * 0.5f;
  spec.flat = iso.backdropFlat;
  spec.n = std::min(kMaxBackdropPills, static_cast<int>(iso.backdropPills.size()));
  const float sx = iso.destW > 0.f ? static_cast<float>(iso.contentW) / iso.destW : pr;
  const float sy = iso.destH > 0.f ? static_cast<float>(iso.contentH) / iso.destH : pr;
  for (int i = 0; i < spec.n; ++i) {
    const BackdropPill& pill = iso.backdropPills[static_cast<std::size_t>(i)];
    spec.pills[i] = plateSpacePill(pill.rect, pill.radius, {0.f, 0.f}, sx, sy);
  }
  return spec;
}

namespace {

void blurSeparable(std::vector<Pixel>& img, int w, int h, float sigma) {
  const int radius = std::max(1, std::min(32, static_cast<int>(std::ceil(3.f * sigma))));
  // Kernel hoisted out of pixel loops; bit-identical output, bounds worst-case cost.
  const float denom = std::max(1.f, sigma);
  std::vector<float> kernel(static_cast<std::size_t>(radius) + 1u);
  for (int k = 0; k <= radius; ++k) {
    const float d = static_cast<float>(k) / denom;
    kernel[static_cast<std::size_t>(k)] = std::exp(-0.5f * d * d);
  }
  thread_local std::vector<Pixel> tmp;
  tmp = img;
  auto blurAxis = [&](bool horiz) {
    const int n = horiz ? w : h;
    const int m = horiz ? h : w;
    for (int j = 0; j < m; ++j) {
      for (int i = 0; i < n; ++i) {
        Pixel acc{0, 0, 0, 0};
        float wt = 0.f;
        for (int k = -radius; k <= radius; ++k) {
          const int p = std::min(n - 1, std::max(0, i + k));
          const float ww = kernel[static_cast<std::size_t>(k < 0 ? -k : k)];
          const Pixel s = horiz ? tmp[static_cast<std::size_t>(j * w + p)]
                                : tmp[static_cast<std::size_t>(p * w + j)];
          acc.r += s.r * ww;
          acc.g += s.g * ww;
          acc.b += s.b * ww;
          acc.a += s.a * ww;
          wt += ww;
        }
        if (wt > 0.f) {
          acc.r /= wt;
          acc.g /= wt;
          acc.b /= wt;
          acc.a /= wt;
        }
        Pixel& out = horiz ? img[static_cast<std::size_t>(j * w + i)]
                           : img[static_cast<std::size_t>(i * w + j)];
        out = acc;
      }
    }
    tmp = img;
  };
  blurAxis(true);
  blurAxis(false);
}

}  // namespace

void blurCheap(std::vector<Pixel>& img, int w, int h, float sigma) {
  if (sigma < 0.45f || w < 2 || h < 2 || img.empty()) {
    return;
  }
  const int scale = sigma >= 18.f ? 4 : (sigma >= 3.f ? 2 : 1);
  if (scale == 1) {
    blurSeparable(img, w, h, sigma);
    return;
  }
  const int dw = std::max(1, w / scale);
  const int dh = std::max(1, h / scale);
  thread_local std::vector<Pixel> small;
  small.assign(static_cast<std::size_t>(dw * dh), Pixel{0, 0, 0, 0});
  for (int y = 0; y < dh; ++y) {
    const int y0 = y * scale;
    const int y1 = std::min(h, y0 + scale);
    for (int x = 0; x < dw; ++x) {
      const int x0 = x * scale;
      const int x1 = std::min(w, x0 + scale);
      Pixel acc{0, 0, 0, 0};
      int n = 0;
      for (int yy = y0; yy < y1; ++yy) {
        for (int xx = x0; xx < x1; ++xx) {
          const Pixel s = img[static_cast<std::size_t>(yy * w + xx)];
          acc.r += s.r;
          acc.g += s.g;
          acc.b += s.b;
          acc.a += s.a;
          ++n;
        }
      }
      if (n > 0) {
        const float inv = 1.f / static_cast<float>(n);
        acc.r *= inv;
        acc.g *= inv;
        acc.b *= inv;
        acc.a *= inv;
      }
      small[static_cast<std::size_t>(y * dw + x)] = acc;
    }
  }
  blurSeparable(small, dw, dh, sigma / static_cast<float>(scale));
  for (int y = 0; y < h; ++y) {
    const float py =
        (static_cast<float>(y) + 0.5f) * static_cast<float>(dh) / static_cast<float>(h);
    for (int x = 0; x < w; ++x) {
      const float px =
          (static_cast<float>(x) + 0.5f) * static_cast<float>(dw) / static_cast<float>(w);
      img[static_cast<std::size_t>(y * w + x)] = samplePixel(small, dw, dh, px, py);
    }
  }
}

void copyDestPlate(
    std::vector<Pixel>& plate,
    int iw,
    int ih,
    const std::vector<Pixel>& dest,
    int dw,
    int dh,
    Rect src
) {
  const float plateW = static_cast<float>(iw);
  const float plateH = static_cast<float>(ih);
  const bool aligned = src.origin.x == static_cast<float>(static_cast<int>(src.origin.x)) &&
                       src.origin.y == static_cast<float>(static_cast<int>(src.origin.y)) &&
                       src.size.x == plateW && src.size.y == plateH;
  if (aligned) {
    const int ox = static_cast<int>(src.origin.x);
    const int oy = static_cast<int>(src.origin.y);
    for (int y = 0; y < ih; ++y) {
      const int dy = std::min(dh - 1, std::max(0, oy + y));
      const Pixel* row = dest.data() + static_cast<std::size_t>(dy * dw);
      Pixel* out = plate.data() + static_cast<std::size_t>(y * iw);
      for (int x = 0; x < iw; ++x) {
        const int dx = std::min(dw - 1, std::max(0, ox + x));
        out[x] = row[dx];
      }
    }
    return;
  }
  for (int y = 0; y < ih; ++y) {
    const float v = (static_cast<float>(y) + 0.5f) / plateH;
    const float sy = src.origin.y + v * src.size.y;
    for (int x = 0; x < iw; ++x) {
      const float u = (static_cast<float>(x) + 0.5f) / plateW;
      const float sx = src.origin.x + u * src.size.x;
      plate[static_cast<std::size_t>(y * iw + x)] = sampleDestBilinear(dest, dw, dh, sx, sy);
    }
  }
}

}  // namespace glim::paint::raster
