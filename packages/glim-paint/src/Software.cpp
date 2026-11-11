#include <glim/assert.h>
#include <glim/paint/ColorSpace.h>
#include <glim/paint/Software.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Font.h"
#include "GroupOrder.h"
#include "LensMath.h"
#include "Parallel.h"
#include "Strip.h"

namespace glim::paint {

thread_local RasterOptions tlsRaster{};

namespace {

struct Pixel {
  float r, g, b, a;
};

// Rec.709 luma of linear RGB.
float luma(const Pixel& p) { return p.r * 0.2126f + p.g * 0.7152f + p.b * 0.0722f; }

void srcOver(Pixel& dst, Pixel src) {
  const float ia = 1.0f - src.a;
  dst.r = src.r + dst.r * ia;
  dst.g = src.g + dst.g * ia;
  dst.b = src.b + dst.b * ia;
  dst.a = src.a + dst.a * ia;
}

void plus(Pixel& dst, Pixel src) {
  dst.r += src.r;
  dst.g += src.g;
  dst.b += src.b;
  dst.a += src.a;
}

inline void blendPixel(Pixel& dst, Pixel src, Blend blend) {
  if (blend == Blend::Plus) {
    plus(dst, src);
  } else {
    srcOver(dst, src);
  }
}

inline Blend quadBlend(const Quad& q, Blend fallback) {
  return (fallback == Blend::Plus || q.plus != 0) ? Blend::Plus : Blend::SrcOver;
}

void fillQuad(
    std::vector<Pixel>& buf, int w, int h, const Quad& q, Blend fallback = Blend::SrcOver
) {
  const Blend blend = quadBlend(q, fallback);
  const float qx0 = q.x;
  const float qy0 = q.y;
  const float qx1 = q.x + q.w;
  const float qy1 = q.y + q.h;
  const int x0 = std::max(0, static_cast<int>(std::floor(qx0)));
  const int y0 = std::max(0, static_cast<int>(std::floor(qy0)));
  const int x1 = std::min(w, static_cast<int>(std::ceil(qx1)));
  const int y1 = std::min(h, static_cast<int>(std::ceil(qy1)));
  if (x0 >= x1 || y0 >= y1) {
    return;
  }
  const bool opaque = q.a >= 0.999f;
  const bool aligned = qx0 == static_cast<float>(x0) && qy0 == static_cast<float>(y0) &&
                       qx1 == static_cast<float>(x1) && qy1 == static_cast<float>(y1);
  if (opaque && aligned && blend == Blend::SrcOver) {
    const Pixel src{q.r, q.g, q.b, 1.f};
    for (int y = y0; y < y1; ++y) {
      Pixel* row = buf.data() + static_cast<std::size_t>(y * w + x0);
      for (int x = 0; x < x1 - x0; ++x) {
        row[x] = src;
      }
    }
    return;
  }
  for (int y = y0; y < y1; ++y) {
    const float cy0 = std::max(static_cast<float>(y), qy0);
    const float cy1 = std::min(static_cast<float>(y + 1), qy1);
    const float ycov = cy1 - cy0;
    if (ycov <= 1e-6f) {
      continue;
    }
    for (int x = x0; x < x1; ++x) {
      const float cx0 = std::max(static_cast<float>(x), qx0);
      const float cx1 = std::min(static_cast<float>(x + 1), qx1);
      const float cov = (cx1 - cx0) * ycov;
      if (cov <= 1e-6f) {
        continue;
      }
      Pixel src{q.r * cov, q.g * cov, q.b * cov, q.a * cov};
      blendPixel(buf[static_cast<std::size_t>(y * w + x)], src, blend);
    }
  }
}

void fillQuads(
    std::vector<Pixel>& buf,
    int w,
    int h,
    const std::vector<Quad>& quads,
    Blend fallback = Blend::SrcOver
) {
  for (const Quad& q : quads) {
    fillQuad(buf, w, h, q, fallback);
  }
}

// CPU mirror of the gradient shader: same t, stops, and coverage multiply at pixel centers.
inline Blend gradientBlend(const GradientQuad& q, Blend fallback) {
  return (fallback == Blend::Plus || q.plus != 0) ? Blend::Plus : Blend::SrcOver;
}

void fillGradientQuad(
    std::vector<Pixel>& buf, int w, int h, const GradientQuad& q, Blend fallback = Blend::SrcOver
) {
  const int n = static_cast<int>(q.stopCount);
  if (n <= 0 || w <= 0 || h <= 0) {
    return;
  }
  const float qx0 = q.x;
  const float qy0 = q.y;
  const float qx1 = q.x + q.w;
  const float qy1 = q.y + q.h;
  const int x0 = std::max(0, static_cast<int>(std::floor(qx0)));
  const int y0 = std::max(0, static_cast<int>(std::floor(qy0)));
  const int x1 = std::min(w, static_cast<int>(std::ceil(qx1)));
  const int y1 = std::min(h, static_cast<int>(std::ceil(qy1)));
  if (x0 >= x1 || y0 >= y1) {
    return;
  }
  const float dx = q.p1x - q.p0x;
  const float dy = q.p1y - q.p0y;
  const float denom = dx * dx + dy * dy;
  const bool linear = q.kind == 0;
  for (int y = y0; y < y1; ++y) {
    const float cy0 = std::max(static_cast<float>(y), qy0);
    const float cy1 = std::min(static_cast<float>(y + 1), qy1);
    const float ycov = cy1 - cy0;
    if (ycov <= 1e-6f) {
      continue;
    }
    for (int x = x0; x < x1; ++x) {
      const float cx0 = std::max(static_cast<float>(x), qx0);
      const float cx1 = std::min(static_cast<float>(x + 1), qx1);
      const float cov = (cx1 - cx0) * ycov * q.coverage;
      if (cov <= 1e-6f) {
        continue;
      }
      const float px = static_cast<float>(x) + 0.5f;
      const float py = static_cast<float>(y) + 0.5f;
      float t;
      if (linear) {
        t = denom > 1e-8f ? ((px - q.p0x) * dx + (py - q.p0y) * dy) / denom : 0.f;
      } else {
        const float rx = px - q.p0x;
        const float ry = py - q.p0y;
        t = q.radius > 1e-6f ? std::hypot(rx, ry) / q.radius : 0.f;
      }
      t = std::min(1.f, std::max(0.f, t));
      float cr = q.r[0];
      float cg = q.g[0];
      float cb = q.b[0];
      float ca = q.a[0];
      for (int i = 1; i < n && i < kMaxGradientStops; ++i) {
        const float o0 = q.offsets[i - 1];
        const float o1 = q.offsets[i];
        float f;
        if (o1 > o0) {
          f = std::min(1.f, std::max(0.f, (t - o0) / (o1 - o0)));
        } else {
          f = t >= o1 ? 1.f : 0.f;
        }
        cr += (q.r[i] - cr) * f;
        cg += (q.g[i] - cg) * f;
        cb += (q.b[i] - cb) * f;
        ca += (q.a[i] - ca) * f;
      }
      Pixel src{cr * cov, cg * cov, cb * cov, ca * cov};
      blendPixel(buf[static_cast<std::size_t>(y * w + x)], src, gradientBlend(q, fallback));
    }
  }
}

void fillGradients(
    std::vector<Pixel>& buf,
    int w,
    int h,
    const std::vector<GradientQuad>& grads,
    Blend fallback = Blend::SrcOver
) {
  for (const GradientQuad& q : grads) {
    fillGradientQuad(buf, w, h, q, fallback);
  }
}

float clipCoverageAt(const ClipState& clip, float px, float py) {
  if (!clip.active) {
    return 1.f;
  }
  if (clip.rect.size.x <= 0.f || clip.rect.size.y <= 0.f) {
    return 0.f;
  }
  return roundedCoverage(clip.rect, clip.radius, px, py);
}

Pixel samplePixel(const std::vector<Pixel>& img, int w, int h, float px, float py);

void blitBuffer(
    std::vector<Pixel>& dest,
    int dw,
    int dh,
    const std::vector<Pixel>& src,
    int sw,
    int sh,
    Rect dstRect,
    float opacity
) {
  const int x0 = std::max(0, static_cast<int>(std::floor(dstRect.origin.x)));
  const int y0 = std::max(0, static_cast<int>(std::floor(dstRect.origin.y)));
  const int x1 = std::min(dw, static_cast<int>(std::ceil(dstRect.origin.x + dstRect.size.x)));
  const int y1 = std::min(dh, static_cast<int>(std::ceil(dstRect.origin.y + dstRect.size.y)));
  const bool aligned = dstRect.origin.x == static_cast<float>(static_cast<int>(dstRect.origin.x)) &&
                       dstRect.origin.y == static_cast<float>(static_cast<int>(dstRect.origin.y)) &&
                       dstRect.size.x == static_cast<float>(sw) &&
                       dstRect.size.y == static_cast<float>(sh);
  const int ox = static_cast<int>(dstRect.origin.x);
  const int oy = static_cast<int>(dstRect.origin.y);
  for (int y = y0; y < y1; ++y) {
    if (aligned) {
      const int sy = std::min(sh - 1, std::max(0, y - oy));
      const Pixel* row = src.data() + static_cast<std::size_t>(sy * sw);
      for (int x = x0; x < x1; ++x) {
        const int sx = std::min(sw - 1, std::max(0, x - ox));
        Pixel p = row[sx];
        p.r *= opacity;
        p.g *= opacity;
        p.b *= opacity;
        p.a *= opacity;
        srcOver(dest[static_cast<std::size_t>(y * dw + x)], p);
      }
      continue;
    }
    const float py = sh <= 1 ? 0.5f
                             : (static_cast<float>(y) + 0.5f - dstRect.origin.y) / dstRect.size.y *
                                   static_cast<float>(sh);
    for (int x = x0; x < x1; ++x) {
      const float px = sw <= 1 ? 0.5f
                               : (static_cast<float>(x) + 0.5f - dstRect.origin.x) /
                                     dstRect.size.x * static_cast<float>(sw);
      Pixel p = samplePixel(src, sw, sh, px, py);
      p.r *= opacity;
      p.g *= opacity;
      p.b *= opacity;
      p.a *= opacity;
      srcOver(dest[static_cast<std::size_t>(y * dw + x)], p);
    }
  }
}

float sampleChannel(const StoredImage& img, float u, float v, int channel) {
  // Texel centers sit at (i + 0.5)/size, so sample at u*size - 0.5 (matches GPU clamp-to-edge).
  const float w = static_cast<float>(std::max(1, img.width));
  const float h = static_cast<float>(std::max(1, img.height));
  const float x = std::clamp(u, 0.f, 1.f) * w - 0.5f;
  const float y = std::clamp(v, 0.f, 1.f) * h - 0.5f;
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int x1 = std::min(std::max(1, img.width) - 1, x0 + 1);
  const int y1 = std::min(std::max(1, img.height) - 1, y0 + 1);
  const float fx = x - static_cast<float>(x0);
  const float fy = y - static_cast<float>(y0);
  auto at = [&](int px, int py) {
    px = std::clamp(px, 0, std::max(0, img.width - 1));
    py = std::clamp(py, 0, std::max(0, img.height - 1));
    return img.rgba[static_cast<std::size_t>((py * img.width + px) * 4 + channel)] / 255.f;
  };
  const float a = at(x0, y0);
  const float b = at(x1, y0);
  const float c = at(x0, y1);
  const float d = at(x1, y1);
  return (a * (1.f - fx) + b * fx) * (1.f - fy) + (c * (1.f - fx) + d * fx) * fy;
}

void blitImage(
    std::vector<Pixel>& dest,
    int w,
    int h,
    const Blit& b,
    const ImageStore& images,
    Vec4 tint,
    const ClipState& clip,
    bool sdf,
    Blend blend = Blend::SrcOver
) {
  auto img = images.get(b.matter.imageId);
  if (!img || img->get().rgba.empty()) {
    return;
  }
  const StoredImage& im = img->get();
  const int x0 = std::max(0, static_cast<int>(std::floor(b.rect.origin.x)));
  const int y0 = std::max(0, static_cast<int>(std::floor(b.rect.origin.y)));
  const int x1 = std::min(w, static_cast<int>(std::ceil(b.rect.origin.x + b.rect.size.x)));
  const int y1 = std::min(h, static_cast<int>(std::ceil(b.rect.origin.y + b.rect.size.y)));
  const float u0 = b.matter.uv.origin.x;
  const float v0 = b.matter.uv.origin.y;
  const float du = b.matter.uv.size.x;
  const float dv = b.matter.uv.size.y;
  const float iw = static_cast<float>(im.width);
  const float texelsPerPx = b.rect.size.x > 1e-6f ? (std::fabs(du) * iw) / b.rect.size.x : 1.f;
  const float aa = std::max(
      0.03f,
      0.6f * texelsPerPx * (static_cast<float>(kSdfOnEdge) / 255.f) / static_cast<float>(kSdfPad)
  );
  for (int y = y0; y < y1; ++y) {
    const float fy =
        b.rect.size.y <= 0 ? 0.f : (static_cast<float>(y) + 0.5f - b.rect.origin.y) / b.rect.size.y;
    const float v = v0 + fy * dv;
    const float cy = static_cast<float>(y) + 0.5f;
    if (cy < b.rect.origin.y || cy >= b.rect.origin.y + b.rect.size.y) {
      continue;
    }
    for (int x = x0; x < x1; ++x) {
      const float cx = static_cast<float>(x) + 0.5f;
      if (cx < b.rect.origin.x || cx >= b.rect.origin.x + b.rect.size.x) {
        continue;
      }
      const float fx = b.rect.size.x <= 0
                           ? 0.f
                           : (static_cast<float>(x) + 0.5f - b.rect.origin.x) / b.rect.size.x;
      const float u = u0 + fx * du;
      Pixel src;
      if (sdf) {
        const float d = sampleChannel(*img, u, v, 0);
        const float t = std::clamp((d - (0.5f - aa)) / std::max(1e-6f, 2.f * aa), 0.f, 1.f);
        const float a = t * t * (3.f - 2.f * t);
        src = {tint.x * a, tint.y * a, tint.z * a, tint.w * a};
      } else {
        // Image bytes are sRGB; the working buffer is linear.
        const float r = srgbToLinear(sampleChannel(*img, u, v, 0));
        const float g = srgbToLinear(sampleChannel(*img, u, v, 1));
        const float b = srgbToLinear(sampleChannel(*img, u, v, 2));
        const float a = sampleChannel(*img, u, v, 3);
        src = {r, g, b, a};
        src.r *= src.a * tint.x;
        src.g *= src.a * tint.y;
        src.b *= src.a * tint.z;
        src.a *= tint.w;
      }
      const float cov =
          clipCoverageAt(clip, static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
      if (cov <= 0.f) {
        continue;
      }
      src.r *= cov;
      src.g *= cov;
      src.b *= cov;
      src.a *= cov;
      blendPixel(dest[static_cast<std::size_t>(y * w + x)], src, blend);
    }
  }
}

void rasterGroup(
    std::vector<Pixel>& dest,
    int w,
    int h,
    const Group& g,
    const ImageStore& images,
    const Mat4& extra,
    const ClipState& parentClip,
    float pixelRatio
);

struct PlateSpec {
  float sigma = 0;
  float mergeK = 0;
  bool flat = false;
  float pr = 1.f;
  int n = 0;
  BackdropPill pills[kMaxBackdropPills]{};
};

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

// A pill moved into plate space: origin-relative, scaled per axis.
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

struct GlassSdf {
  float d = 1e6f;
  float nx = 0.f;
  float ny = 1.f;
  float dome = 1.f;    // min half extent of the nearest pill (px)
  float corner = 0.f;  // clamped corner radius of the nearest pill (px)
  float fillet = 0.f;  // 1 in the curved corners/caps, 0 along flat sides
};

// Mirrors filletWeight() in the lens shaders for one pill.
float pillFillet(float px, float py, float cx, float cy, float hx, float hy, float r) {
  const float rr = std::max(r, 1.f);
  if (r >= std::min(hx, hy) - 0.5f) {
    // Curvature lives only in the end caps: distance past the straight
    // segment, ramped smoothly from half a radius before the seam.
    const float past = hx >= hy ? std::fabs(px - cx) - (hx - r) : std::fabs(py - cy) - (hy - r);
    return lens::smoothstep(-0.5f * rr, rr, past);
  }
  const float fx = std::fabs(px - cx) - (hx - r);
  const float fy = std::fabs(py - cy) - (hy - r);
  return lens::saturate(fx / rr) * lens::saturate(fy / rr);
}

GlassSdf sdRoundBoxN(float px, float py, float hx, float hy, float r) {
  r = std::min(r, std::min(hx, hy));
  const float ax = std::fabs(px);
  const float ay = std::fabs(py);
  const float qx = ax - hx + r;
  const float qy = ay - hy + r;
  const float sx = px < 0.f ? -1.f : 1.f;
  const float sy = py < 0.f ? -1.f : 1.f;
  GlassSdf o;
  if (qx > 0.f && qy > 0.f) {
    const float len = std::sqrt(qx * qx + qy * qy);
    o.d = len - r;
    const float inv = 1.f / std::max(len, 1e-4f);
    o.nx = qx * inv * sx;
    o.ny = qy * inv * sy;
  } else if (qx > qy) {
    o.d = qx - r;
    o.nx = sx;
    o.ny = 0.f;
  } else {
    o.d = qy - r;
    o.nx = 0.f;
    o.ny = sy;
  }
  return o;
}

GlassSdf glassFieldEval(const PlateSpec& spec, float px, float py) {
  GlassSdf best;
  const int n = spec.n > 0 ? spec.n : 1;
  for (int i = 0; i < n; ++i) {
    const BackdropPill& pill = spec.pills[i];
    const float hx = pill.rect.size.x * 0.5f;
    const float hy = pill.rect.size.y * 0.5f;
    const float cx = pill.rect.origin.x + hx;
    const float cy = pill.rect.origin.y + hy;
    const float bx = std::fabs(px - cx) - hx;
    const float by = std::fabs(py - cy) - hy;
    // The merge neck lives up to k outside a pill, so only cull beyond that.
    const float reach = 1.4f + spec.mergeK * 3.5f;
    if (bx > reach || by > reach) {
      const float outside = bx > by ? bx : by;
      if (i == 0 || outside < best.d) {
        best.d = outside;
        best.nx = 0.f;
        best.ny = 1.f;
      }
      continue;
    }
    GlassSdf s = sdRoundBoxN(px - cx, py - cy, hx, hy, pill.radius);
    if (spec.mergeK > 0.001f && i > 0) {
      s.d = lens::sminCirc(best.d, s.d, spec.mergeK);
    }
    if (i == 0 || s.d < best.d) {
      best = s;
      best.dome = std::min(hx, hy);
      best.corner = std::min(pill.radius, best.dome);
      best.fillet = pillFillet(px, py, cx, cy, hx, hy, best.corner);
    }
  }
  return best;
}

void waterRipple(float px, float py, float iw, float ih, const Glass& mat, float& rx, float& ry) {
  rx = 0.f;
  ry = 0.f;
  if (!glassRipples(mat)) {
    return;
  }
  constexpr float kTau = 6.28318f;
  const float ph1 = kTau * (px / iw * 3.f + py / ih * 2.f + mat.timeSec * 0.12f);
  const float ph2 = kTau * (px / iw * 5.f - py / ih * 3.f - mat.timeSec * 0.09f);
  rx = mat.rippleAmp * (std::cos(ph1) + 0.6f * std::cos(ph2));
  ry = mat.rippleAmp * (std::sin(ph1 * 1.3f) + 0.6f * std::sin(ph2 * 1.1f));
}

// The material's swell at plate pixel (px, py), in dome-radius units.
lens::Swell plateSwell(const Glass& mat, float px, float py, float dome) {
  return lens::swellAt(
      px / dome * mat.swellScale.x, py / dome * mat.swellScale.y, glassSwellPhase(mat)
  );
}

// Dome normal folded by the swell + mirror reflection vector for the
// opaque liquid-metal plate. tilt: the unfolded dome tilt (1 at the rim).
struct MetalNormal {
  float nx, ny, nz, rx, ry, tilt;
};

MetalNormal metalNormal(const GlassSdf& f, const Glass& mat, float px, float py) {
  const float inside = std::max(-f.d, 0.f);
  const float dome = std::max(f.dome, 1.f);
  const float tilt = lens::domeTilt(inside, dome) * 0.97f;
  // Liquid folds: the swell stretched along the plate, full in the body and
  // gone at the rim so the bevel stays clean.
  const lens::Swell sw = plateSwell(mat, px, py, dome);
  const float fold = mat.swellAmp * (1.f - tilt);
  MetalNormal m;
  m.tilt = tilt;
  m.nx = f.nx * tilt + sw.sx * fold;
  m.ny = f.ny * tilt + sw.sy * fold;
  const float len = std::sqrt(m.nx * m.nx + m.ny * m.ny);
  if (len > 0.97f) {
    m.nx *= 0.97f / len;
    m.ny *= 0.97f / len;
  }
  m.nz = std::sqrt(std::max(1.f - m.nx * m.nx - m.ny * m.ny, 0.f));
  // R = reflect(view=(0,0,-1), N).xy
  m.rx = 2.f * m.nz * m.nx;
  m.ry = 2.f * m.nz * m.ny;
  return m;
}

// Metal F0 from its tint, normalized so natural silver reads near-white and
// gold stays saturated; untinted metal is white.
Pixel metalF0(const Glass& mat) {
  const bool tinted = mat.tint.a() != 0;
  const float tr = tinted ? srgbToLinear(mat.tint.red()) : 1.f;
  const float tg = tinted ? srgbToLinear(mat.tint.green()) : 1.f;
  const float tb = tinted ? srgbToLinear(mat.tint.blue()) : 1.f;
  const float k = 0.95f / std::max({tr, tg, tb, 1e-4f});
  return Pixel{tr * k, tg * k, tb * k, 1.f};
}

// envL: brightness of the surroundings along the reflected ray (used only
// when envLight > 0). Chrome: the face mirrors the studio room, never the
// backdrop. Toward the rim the channels split (dispersion) into a chromatic
// fringe, and the rolled rim (lens::chromeRim) lights the upper lip and
// darkens the lower one, splitting where they meet.
Pixel metalEnvGlass(const GlassSdf& f, const Glass& mat, float envL, float px, float py) {
  const MetalNormal m = metalNormal(f, mat, px, py);
  const float lit = 0.8f + mat.envLight * envL;
  const float t2 = m.tilt * m.tilt;
  const float split = mat.dispersion * 0.6f * t2 * t2;
  const float rimW = lens::smoothstep(0.65f, 0.97f, m.tilt) * 0.85f;
  const auto studio = [&](float ry, float shift) {
    const float l = lens::chromeStudio(ry + shift) * lit;
    return l + (lens::chromeRim(f.ny + 2.f * shift) * lit - l) * rimW;
  };
  const Pixel f0 = metalF0(mat);
  // Key-light strike (same H as the glass specular path) + bevel ring.
  const lens::Vec3 h = lens::keyHalfVector();
  const float ndh = std::clamp(m.nx * h.x + m.ny * h.y + m.nz * h.z, 0.f, 1.f);
  const float spec = std::pow(ndh, 56.f) * 0.9f;
  const float bevel = lens::hairWeight(std::max(-f.d, 0.f), 1.2f, 1.6f) * 0.55f;
  const float r = studio(m.ry, -split) * lens::fresnelSchlick(f0.r, m.nz) + spec;
  const float g = studio(m.ry, 0.f) * lens::fresnelSchlick(f0.g, m.nz) + spec;
  const float b = studio(m.ry, split) * lens::fresnelSchlick(f0.b, m.nz) + spec;
  return Pixel{r + (1.f - r) * bevel, g + (1.f - g) * bevel, b + (1.f - b) * bevel, 1.f};
}

Pixel shadeGlass(
    Pixel p, const GlassSdf& f, const Glass& mat, float mask, float px, float py, float iw, float ih
) {
  const bool flatten = mat.flatten || mat.variant == GlassVariant::Identity;
  if (flatten) {
    p.r = 0.94f;
    p.g = 0.94f;
    p.b = 0.94f;
    p.a = 1.f;
    const float rim = std::clamp(1.6f - std::fabs(f.d), 0.f, 1.f) * 0.05f;
    p.r = p.r + (1.f - p.r) * rim;
    p.g = p.g + (1.f - p.g) * rim;
    p.b = p.b + (1.f - p.b) * rim;
  } else {
    const bool isGlass = mat.material == GlassMaterial::Glass;
    const bool isMetal = mat.material == GlassMaterial::LiquidMetal;
    const bool isWater = mat.material == GlassMaterial::Water;
    const bool regular = mat.variant == GlassVariant::Regular;
    float rx = 0.f;
    float ry = 0.f;
    if (isWater) {
      waterRipple(px, py, iw, ih, mat, rx, ry);
    }
    if (isMetal) {
      p = metalEnvGlass(f, mat, luma(p), px, py);
    } else {
      if (isGlass && mat.tint.a() != 0) {
        // Tinted plate: the wash takes the tint color at its alpha.
        // Transparent keeps the white milk. Plate is linear.
        const float ta = mat.tint.alpha();
        const float tr = srgbToLinear(mat.tint.red());
        const float tg = srgbToLinear(mat.tint.green());
        const float tb = srgbToLinear(mat.tint.blue());
        p.r = p.r + (tr - p.r) * ta;
        p.g = p.g + (tg - p.g) * ta;
        p.b = p.b + (tb - p.b) * ta;
      } else if (isGlass) {
        // 0.04 linear ≈ old 0.22 sRGB wash (0.008 for Clear).
        const float milk = regular ? 0.04f : 0.008f;
        p.r = p.r + (1.f - p.r) * milk;
        p.g = p.g + (1.f - p.g) * milk;
        p.b = p.b + (1.f - p.b) * milk;
      }
    }
    // Water: depth absorption in the tint; swell crests focus light.
    if (isWater) {
      const float insideW = std::max(-f.d, 0.f);
      const float dome = std::max(f.dome, 1.f);
      const float depth = lens::smoothstep(0.f, dome, insideW);
      const float ab = 0.2f * lens::saturate(mat.absorbK) * depth;
      const float tr = srgbToLinear(mat.tint.red());
      const float tg = srgbToLinear(mat.tint.green());
      const float tb = srgbToLinear(mat.tint.blue());
      const lens::Swell sw = plateSwell(mat, px, py, dome);
      const float focus = 1.f + 0.23f * mat.swellAmp * sw.curv * depth;
      p.r = (p.r + (p.r * tr * 1.6f - p.r) * ab) * focus;
      p.g = (p.g + (p.g * tg * 1.6f - p.g) * ab) * focus;
      p.b = (p.b + (p.b * tb * 1.6f - p.b) * ab) * focus;
    }
    // Clear dimmer: 35% darkening when plate luma exceeds 0.6 (spec S9).
    if (isGlass && !regular) {
      const float lum = luma(p);
      if (lum > 0.6f) {
        p.r *= 0.65f;
        p.g *= 0.65f;
        p.b *= 0.65f;
      }
    }
    // Edge light is per-side; signed side weights match the old
    // symmetric darkening exactly when both sides are set.
    if (mask > 0.85f && f.d > -5.f) {
      const float inside = std::max(-f.d, 0.f);
      const float hair2 = lens::hairWeight(inside, 1.2f, 1.8f);
      float side = 0.f;
      if (hasEdge(mat.edge, GlassEdge::Left)) {
        const float w = std::clamp(-f.nx, 0.f, 1.f);
        side += w * w;
      }
      if (hasEdge(mat.edge, GlassEdge::Right)) {
        const float w = std::clamp(f.nx, 0.f, 1.f);
        side += w * w;
      }
      p.r *= 1.f - side * hair2 * 0.38f;
      p.g *= 1.f - side * hair2 * 0.38f;
      p.b *= 1.f - side * hair2 * 0.38f;
      const float top = hasEdge(mat.edge, GlassEdge::Top) ? std::clamp(-f.ny, 0.f, 1.f) : 0.f;
      const float bot = hasEdge(mat.edge, GlassEdge::Bottom) ? std::clamp(f.ny, 0.f, 1.f) : 0.f;
      const float hi = hair2 * (top * 0.35f + bot * 0.22f) * 0.9f;
      p.r = p.r + (1.f - p.r) * hi;
      p.g = p.g + (1.f - p.g) * hi;
      p.b = p.b + (1.f - p.b) * hi;
      // The soft all-around rim belongs to the full jeweled plate;
      // partial masks carry their look in the directional terms above.
      if (mat.edge == GlassEdge::Full) {
        const float rim = std::clamp(1.f - std::fabs(f.d + 0.4f) / 2.4f, 0.f, 1.f);
        const float rimA = rim * rim * 0.12f;
        p.r = p.r + (1.f - p.r) * rimA;
        p.g = p.g + (1.f - p.g) * rimA;
        p.b = p.b + (1.f - p.b) * rimA;
      }
      // Body Fresnel (Schlick F0 0.04) + key-light specular at -60 deg.
      // GPU parity: shininess 56, convergence 0.7, edge-gated. Metal
      // carries its own in metalEnvGlass.
      if (!isMetal) {
        const float nl = std::max(1e-4f, std::sqrt(f.nx * f.nx + f.ny * f.ny + 1.f));
        const float nz = 1.f / nl;
        const float fres = lens::fresnelSchlick(0.04f, nz);
        const float inside = std::max(-f.d, 0.f);
        const float rimW = std::clamp(1.f - inside / 12.f, 0.f, 1.f);
        float edgeSpec = 0.f;
        if (hasEdge(mat.edge, GlassEdge::Left)) edgeSpec += std::clamp(-f.nx, 0.f, 1.f);
        if (hasEdge(mat.edge, GlassEdge::Right)) edgeSpec += std::clamp(f.nx, 0.f, 1.f);
        if (hasEdge(mat.edge, GlassEdge::Top)) edgeSpec += std::clamp(-f.ny, 0.f, 1.f);
        if (hasEdge(mat.edge, GlassEdge::Bottom)) edgeSpec += std::clamp(f.ny, 0.f, 1.f);
        edgeSpec = std::clamp(edgeSpec, 0.f, 1.f);
        const auto [hx, hy, hz] = lens::keyHalfVector();
        const float nx3 = (f.nx * 0.9f + (isWater ? rx * 0.18f : 0.f)) / nl;
        const float ny3 = (f.ny * 0.9f + (isWater ? ry * 0.18f : 0.f)) / nl;
        const float nz3 = nz;
        const float ndh = std::clamp(
            (nx3 * hx + ny3 * hy + nz3 * hz) /
                std::max(1e-4f, std::sqrt(nx3 * nx3 + ny3 * ny3 + nz3 * nz3) * 1.f),
            0.f,
            1.f
        );
        float spec = std::pow(ndh, 56.f) * 0.9f;
        spec *= 1.f - 0.3f * (1.f - rimW);
        spec *= edgeSpec;
        const float fresWash = fres * rimW * edgeSpec;
        // Fade from 3px so the 5px gate above leaves no hard line.
        const float edgeBand = 1.f - lens::smoothstep(3.f, 5.f, inside);
        const float add = (fresWash * 0.30f + spec * (0.35f + 0.65f * rimW)) * edgeBand;
        p.r += add;
        p.g += add;
        p.b += add;
        if (isWater) {
          // Crest lobe only for rippling water; still water has no crests.
          const float crest =
              mat.rippleAmp > 0.f ? std::clamp((rx + ry) * 0.25f + 0.5f, 0.f, 1.f) : 0.f;
          const float ca = std::pow(rimW * (1.f - rimW) * 4.f, 2.f);
          const float cw = ca * 0.14f * crest;
          p.r += 0.85f * cw;
          p.g += 1.0f * cw;
          p.b += 0.92f * cw;
        }
      }
    }
  }
  p.r *= mask;
  p.g *= mask;
  p.b *= mask;
  p.a *= mask;
  return p;
}

void maskFrostPlate(std::vector<Pixel>& plate, int iw, int ih, const PlateSpec& spec) {
  const float frost = spec.flat ? 0.f : 0.07f + 0.12f * std::clamp(spec.sigma / 16.f, 0.f, 1.f);
  PlateSpec field = spec;
  if (field.n <= 0) {
    field.n = 1;
    field.pills[0].rect = Rect::fromSize({static_cast<float>(iw), static_cast<float>(ih)});
  }
  for (int y = 0; y < ih; ++y) {
    const float py = static_cast<float>(y) + 0.5f;
    for (int x = 0; x < iw; ++x) {
      const float px = static_cast<float>(x) + 0.5f;
      const GlassSdf f = glassFieldEval(field, px, py);
      const float mask = std::clamp((1.5f - f.d) / 1.5f, 0.f, 1.f);
      const std::size_t i = static_cast<std::size_t>(y * iw + x);
      if (mask < 0.001f) {
        plate[i] = Pixel{0, 0, 0, 0};
        continue;
      }
      Pixel p = plate[i];
      if (spec.flat) {
        const float lum = luma(p);
        const float tint = lum >= 0.45f ? 0.92f : 0.16f;
        p.r = tint;
        p.g = tint;
        p.b = tint;
        p.a = 1.f;
      } else if (frost > 0.f) {
        p.r = p.r + (1.f - p.r) * frost;
        p.g = p.g + (1.f - p.g) * frost;
        p.b = p.b + (1.f - p.b) * frost;
      }
      p.r *= mask;
      p.g *= mask;
      p.b *= mask;
      p.a *= mask;
      plate[i] = p;
    }
  }
}

void applyGlassChrome(
    std::vector<Pixel>& plate, int iw, int ih, const PlateSpec& field, const Glass& mat
) {
  for (int y = 0; y < ih; ++y) {
    const float py = static_cast<float>(y) + 0.5f;
    for (int x = 0; x < iw; ++x) {
      const float px = static_cast<float>(x) + 0.5f;
      const GlassSdf f = glassFieldEval(field, px, py);
      const float mask = std::clamp((1.5f - f.d) / 1.5f, 0.f, 1.f);
      const std::size_t i = static_cast<std::size_t>(y * iw + x);
      if (mask < 0.001f) {
        plate[i] = Pixel{0, 0, 0, 0};
        continue;
      }
      const float cx = static_cast<float>(x) + 0.5f;
      plate[i] = shadeGlass(plate[i], f, mat, mask, cx, py, iw, ih);
    }
  }
}

PlateSpec glassPillsToField(
    const GlassPill* pills, int n, float mergeKPx, float pr, const Rect& surface
) {
  PlateSpec spec;
  spec.mergeK = tlsRaster.glassMerge ? mergeKPx * pr : 0.f;
  spec.pr = pr;
  if (n <= 0) {
    spec.n = 1;
    spec.pills[0].rect = Rect::fromSize({surface.size.x * pr, surface.size.y * pr});
    spec.pills[0].radius = std::min(spec.pills[0].rect.size.x, spec.pills[0].rect.size.y) * 0.5f;
    return spec;
  }
  spec.n = std::min(kMaxBackdropPills, n);
  for (int i = 0; i < spec.n; ++i) {
    spec.pills[i] = plateSpacePill(pills[i].rect, pills[i].radius, surface.origin, pr, pr);
  }
  return spec;
}

Pixel sampleCheapFrost(
    const std::vector<Pixel>& dest, int dw, int dh, float sx, float sy, float radius
) {
  if (radius < 0.45f) {
    return sampleDestBilinear(dest, dw, dh, sx, sy);
  }
  Pixel acc{0, 0, 0, 0};
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      const Pixel s = sampleDest(
          dest, dw, dh, sx + static_cast<float>(dx) * radius, sy + static_cast<float>(dy) * radius
      );
      acc.r += s.r;
      acc.g += s.g;
      acc.b += s.b;
      acc.a += s.a;
    }
  }
  const float inv = 1.f / 9.f;
  return Pixel{acc.r * inv, acc.g * inv, acc.b * inv, acc.a * inv};
}

void sampleGlassPlate(
    std::vector<Pixel>& plate,
    int iw,
    int ih,
    const std::vector<Pixel>& dest,
    int dw,
    int dh,
    Rect src,
    const Glass& mat,
    const PlateSpec& field
) {
  if (iw <= 0 || ih <= 0 || dw <= 0 || dh <= 0 || src.size.x <= 0.f || src.size.y <= 0.f) {
    return;
  }
  const bool flatten = mat.flatten || mat.variant == GlassVariant::Identity;
  const bool metal = !flatten && glassOpaque(mat);
  if (metal) {
    const float plateW = static_cast<float>(iw);
    const float plateH = static_cast<float>(ih);
    const float kx = src.size.x / plateW;
    const float ky = src.size.y / plateH;
    for (int y = 0; y < ih; ++y) {
      const float py = static_cast<float>(y) + 0.5f;
      for (int x = 0; x < iw; ++x) {
        const float px = static_cast<float>(x) + 0.5f;
        const GlassSdf f = glassFieldEval(field, px, py);
        const float mask = std::clamp((1.5f - f.d) / 1.5f, 0.f, 1.f);
        const std::size_t i = static_cast<std::size_t>(y * iw + x);
        if (mask < 0.001f) {
          plate[i] = Pixel{0, 0, 0, 0};
          continue;
        }
        // envLight: the surroundings' brightness along the reflected ray
        // (reach 2.2x dome; envSoft widens the blur) lights the studio.
        Pixel env{0, 0, 0, 0};
        if (mat.envLight > 0.f) {
          const MetalNormal m = metalNormal(f, mat, px, py);
          const float reach = f.dome * 2.2f;
          const float bx = src.origin.x + px / plateW * src.size.x + m.rx * reach * kx;
          const float by = src.origin.y + py / plateH * src.size.y + m.ry * reach * ky;
          const float rough = 1.f + mat.envSoft * 4.f * kx;
          env = sampleCheapFrost(dest, dw, dh, bx, by, rough);
        }
        plate[i] = shadeGlass(env, f, mat, mask, px, py, plateW, plateH);
      }
    }
    return;
  }
  if (tlsRaster.glassBlur && !flatten) {
    copyDestPlate(plate, iw, ih, dest, dw, dh, src);
    const float sigma = mat.material == GlassMaterial::Water
                            ? 1.f
                            : (mat.variant == GlassVariant::Clear ? 0.f : 6.f);
    blurCheap(plate, iw, ih, sigma);
    applyGlassChrome(plate, iw, ih, field, mat);
    return;
  }
  const float plateW = static_cast<float>(iw);
  const float plateH = static_cast<float>(ih);
  const float frostR = flatten ? 0.f : (mat.variant == GlassVariant::Regular ? 3.25f : 0.f);
  const bool aligned = src.origin.x == static_cast<float>(static_cast<int>(src.origin.x)) &&
                       src.origin.y == static_cast<float>(static_cast<int>(src.origin.y)) &&
                       src.size.x == plateW && src.size.y == plateH;
  const int ox = static_cast<int>(src.origin.x);
  const int oy = static_cast<int>(src.origin.y);
  const bool regular = mat.variant == GlassVariant::Regular;
  const float prN = std::max(field.pr, 1.f);
  const float kx = src.size.x / plateW;
  const float ky = src.size.y / plateH;
  float thickPx = 0.f;
  if (!flatten && field.n > 0) {
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int k = 0; k < field.n; ++k) {
      const Rect& r = field.pills[k].rect;
      x0 = std::min(x0, r.origin.x);
      y0 = std::min(y0, r.origin.y);
      x1 = std::max(x1, r.origin.x + r.size.x);
      y1 = std::max(y1, r.origin.y + r.size.y);
    }
    const float ipr = 1.f / std::max(field.pr, 1e-4f);
    thickPx =
        glassThicknessFor(mat, Rect{{x0 * ipr, y0 * ipr}, {(x1 - x0) * ipr, (y1 - y0) * ipr}}) *
        field.pr;
  }
  const bool refractive = thickPx > 0.001f;
  for (int y = 0; y < ih; ++y) {
    const float py = static_cast<float>(y) + 0.5f;
    const float sy = aligned ? 0.f : src.origin.y + py / plateH * src.size.y;
    for (int x = 0; x < iw; ++x) {
      const float px = static_cast<float>(x) + 0.5f;
      const GlassSdf f = glassFieldEval(field, px, py);
      const float mask = std::clamp((1.5f - f.d) / 1.5f, 0.f, 1.f);
      const std::size_t i = static_cast<std::size_t>(y * iw + x);
      if (mask < 0.001f) {
        plate[i] = Pixel{0, 0, 0, 0};
        continue;
      }
      float sxr = 0.f;
      float syr = 0.f;
      if (!flatten && mat.material == GlassMaterial::Water) {
        // Body lens: a shallow dome of water magnifies what lies under it.
        // Inward shift falls linearly from the rim to the dome center, so the
        // map stays monotonic (no fold/double image) and magnifies by
        // R / (R - bend). Snell sets bend from ior; capped below the radius.
        const float tilt = lens::domeTilt(std::max(-f.d, 0.f), f.dome);
        const float bend =
            std::min((1.f - 1.f / std::max(mat.ior, 1.01f)) * thickPx * 1.6f, f.dome * 0.45f);
        sxr -= f.nx * tilt * bend * kx;
        syr -= f.ny * tilt * bend * ky;
        // The surface swell bends what lies beneath into soft waves.
        const float dome = std::max(f.dome, 1.f);
        const lens::Swell sw = plateSwell(mat, px, py, dome);
        sxr += sw.sx * bend * mat.swellAmp * kx;
        syr += sw.sy * bend * mat.swellAmp * ky;
      }
      // Ripple wobbles only the rim's refracted sample, as on the GPU; the
      // pane keeps the body lens alone.
      float ripX = 0.f;
      float ripY = 0.f;
      if (glassRipples(mat)) {
        waterRipple(px, py, plateW, plateH, mat, ripX, ripY);
        ripX *= src.size.x / plateW * 0.6f;
        ripY *= src.size.y / plateH * 0.6f;
      }
      Pixel p;
      if (aligned) {
        const float sx = static_cast<float>(ox + x) + 0.5f + sxr;
        const float syc = static_cast<float>(oy + y) + 0.5f + syr;
        p = frostR > 0.f ? sampleCheapFrost(dest, dw, dh, sx, syc, frostR)
                         : sampleDest(dest, dw, dh, sx, syc);
      } else {
        const float sx = src.origin.x + px / plateW * src.size.x + sxr;
        p = frostR > 0.f ? sampleCheapFrost(dest, dw, dh, sx, sy + syr, frostR)
                         : sampleDestBilinear(dest, dw, dh, sx, sy + syr);
      }
      if (refractive) {
        const float delta = std::max(-f.d, 0.f);
        const float wrapT =
            12.f * prN + (std::max(thickPx, f.corner * 0.6f) - 12.f * prN) * f.fillet;
        const float diag = lens::saturate(std::fabs(f.nx * f.ny) * 2.f);
        const lens::RimRefract rr =
            lens::rimRefract(delta, wrapT, mat.ior, mat.refDistance, prN, f.fillet, diag);
        if (rr.wrapX > 0.f) {
          const float bx = (aligned ? static_cast<float>(ox + x) + 0.5f
                                    : src.origin.x + px / plateW * src.size.x) +
                           sxr + ripX;
          const float by = (aligned ? static_cast<float>(oy + y) + 0.5f : sy) + syr + ripY;
          const float ox0 = f.nx * rr.magPx * kx;
          const float oy0 = f.ny * rr.magPx * ky;
          Pixel w = sampleDestBilinear(dest, dw, dh, bx + ox0, by + oy0);
          const float gamma = mat.dispersion;
          if (gamma > 0.f && rr.wrapX > 0.08f) {
            w.r =
                sampleDestBilinear(dest, dw, dh, bx + ox0 * (1.f - gamma), by + oy0 * (1.f - gamma))
                    .r;
            w.b =
                sampleDestBilinear(dest, dw, dh, bx + ox0 * (1.f + gamma), by + oy0 * (1.f + gamma))
                    .b;
          }
          if (frostR > 0.f) {
            const Pixel fz = sampleCheapFrost(dest, dw, dh, bx + ox0, by + oy0, frostR);
            w.r += (fz.r - w.r) * 0.88f;
            w.g += (fz.g - w.g) * 0.88f;
            w.b += (fz.b - w.b) * 0.88f;
          }
          const float wm = regular ? std::pow(rr.wrapX, 1.25f) : rr.wrapX;
          p.r += (w.r - p.r) * wm;
          p.g += (w.g - p.g) * wm;
          p.b += (w.b - p.b) * wm;
        }
      }
      plate[i] = shadeGlass(p, f, mat, mask, px, py, plateW, plateH);
    }
  }
}

bool bakesQuads(const Shape& s) {
  return !std::get_if<Blit>(&s) && !std::get_if<GlyphRun>(&s) && !std::get_if<SlotHole>(&s);
}

struct FlatShape {
  std::vector<Quad> quads;
  std::vector<GradientQuad> grads;
};

FlatShape flattenShape(const Shape& s, const Mat4& world, const ClipState& clip) {
  FlatShape f;
  std::vector<BlitQuad> unused;
  appendShapeWorld(f.quads, unused, f.grads, s, world, clip);
  return f;
}

constexpr std::size_t kParallelShapes = 256;

std::vector<FlatShape> flattenShapes(const Group& g, const Mat4& world, const ClipState& clip) {
  std::vector<FlatShape> flat;
  if (g.shapes.size() < kParallelShapes) return flat;
  flat.resize(g.shapes.size());
  parallelFor(g.shapes.size(), [&](std::size_t i) {
    if (bakesQuads(g.shapes[i])) flat[i] = flattenShape(g.shapes[i], world, clip);
  });
  return flat;
}

void paintShapes(
    std::vector<Pixel>& dest,
    int w,
    int h,
    const Group& g,
    const ImageStore& images,
    const Mat4& world,
    const ClipState& clip,
    float pixelRatio
) {
  std::vector<FlatShape> flat = flattenShapes(g, world, clip);
  GroupOrder runs;
  visitGroup(
      g,
      [&](const Shape& s) {
        const Blend blend = g.params.blend;
        // Blit/glyph fast path keeps AABB bake (now correct AABB);
        // rotated quads go through the world-aware stripper.
        if (!bakesQuads(s)) {
          const Shape xf = transformShape(world, s);
          if (const auto* blit = std::get_if<Blit>(&xf)) {
            blitImage(
                dest, w, h, *blit, images, premulLinear(blit->matter.color), clip, false, blend
            );
            return;
          }
          if (std::get_if<SlotHole>(&xf)) {
            return;
          }
          if (std::get_if<GlyphRun>(&xf)) {
            std::vector<Quad> unused;
            std::vector<BlitQuad> blits;
            std::vector<GradientQuad> unusedGrads;
            appendShape(unused, blits, unusedGrads, xf, clip);
            for (const BlitQuad& q : blits) {
              Blit b;
              b.rect = {{q.x, q.y}, {q.w, q.h}};
              b.matter.kind = MatterKind::Sampled;
              b.matter.imageId = q.imageId;
              b.matter.uv = {{q.u0, q.v0}, {q.u1 - q.u0, q.v1 - q.v0}};
              blitImage(dest, w, h, b, images, {q.r, q.g, q.b, q.a}, clip, q.sdf != 0, blend);
            }
            return;
          }
        }
        const FlatShape f = flat.empty()
                                ? flattenShape(s, world, clip)
                                : std::move(flat[static_cast<std::size_t>(&s - g.shapes.data())]);
        fillQuads(dest, w, h, f.quads, blend);
        fillGradients(dest, w, h, f.grads, blend);
      },
      [&](const Group& child) {
        const bool backdrop = hasBackdrop(child);
        const bool glassWork = hasGlassWork(child);
        runs.next(backdrop, glassWork);
        rasterGroup(dest, w, h, child, images, world, clip, pixelRatio);
      }
  );
}

void rasterGroup(
    std::vector<Pixel>& dest,
    int w,
    int h,
    const Group& g,
    const ImageStore& images,
    const Mat4& extra,
    const ClipState& parentClip,
    float pixelRatio
) {
  const Mat4 world = extra * g.params.transform;
  const ClipState clip = intersectClip(parentClip, clipOf(g.params, world));
  if (needsIsolate(g)) {
    const bool backdrop = hasBackdrop(g);
    const bool glassWork = hasGlassWork(g);
    const bool shadow = hasShadow(g);
    const bool content = hasContentBlur(g);
    const bool explicitBounds = g.params.bounds.size.x > 0.f && g.params.bounds.size.y > 0.f;
    const Rect surface =
        glassWork
            ? glassSurface(g)
            : (hasBackdrop(g)
                   ? backdropSurface(g)
                   : ((shadow || content) ? shadowContentSurface(g)
                                          : (explicitBounds ? g.params.bounds : contentBounds(g))));
    const float pr = pixelRatio > 0.f ? pixelRatio : 1.f;
    const float effPr = useHalfIsolate(g) ? pr * 0.5f : pr;
    const int iw = isolatePixelSize(surface.size.x, effPr);
    const int ih = isolatePixelSize(surface.size.y, effPr);
    std::vector<Pixel> tmp(static_cast<std::size_t>(iw * ih), Pixel{0, 0, 0, 0});
    const Rect xf = transformRect(world, surface);
    if (glassWork) {
      const Glass& mat = glassMaterial(g);
      GlassPill pills[kMaxGlassPills];
      const int n = collectGlassPills(g, pills);
      if (n > 0) {
        const PlateSpec field = glassPillsToField(pills, n, mat.mergeKPx, pr, surface);
        sampleGlassPlate(tmp, iw, ih, dest, w, h, xf, mat, field);
      }
    } else {
      const float sigma = snapBackdropSigma(g.params.backdropBlur);
      if (sigma > 0.f) {
        PlateSpec copy = specFromGroup(g, surface, pr);
        copyDestPlate(tmp, iw, ih, dest, w, h, xf);
        blurCheap(tmp, iw, ih, sigma * pr * 0.5f);
        maskFrostPlate(tmp, iw, ih, copy);
      }
    }
    auto local = cloneGroup(g);
    local->params.opacity = 1.0f;
    local->params.isolate = false;
    clearBackdropParams(local->params);
    clearShadowParams(local->params);
    local->params.transform = Mat4::identity();
    if (backdrop) {
      dropBackdropPills(*local);
    }
    if (glassWork) {
      stripGlassForIsolate(*local);
    }
    if (!hasClip(local->params) && surface.size.x > 0.f && surface.size.y > 0.f) {
      local->params.clip = surface;
    }
    const Mat4 localX =
        Mat4::scale(effPr, effPr) * Mat4::translate(-surface.origin.x, -surface.origin.y);
    const ClipState localClip = clipOf(local->params, localX);
    if (shadow || content) {
      std::vector<Pixel> card(static_cast<std::size_t>(iw * ih), Pixel{0, 0, 0, 0});
      paintShapes(card, iw, ih, *local, images, localX, localClip, effPr);
      if (content) {
        const float csigma = snapContentSigma(g.params.contentBlur) * effPr * 0.5f;
        blurCheap(card, iw, ih, csigma);
      }
      if (shadow && g.params.shadow.has_value()) {
        const ShadowBlur& sb = *g.params.shadow;
        const Vec4 tint = premulLinear(sb.color);
        std::vector<Pixel> sh(static_cast<std::size_t>(iw * ih), Pixel{0, 0, 0, 0});
        for (std::size_t i = 0; i < card.size() && i < sh.size(); ++i) {
          const float a = card[i].a;
          if (a > 1e-5f) {
            sh[i].r = tint.x * a;
            sh[i].g = tint.y * a;
            sh[i].b = tint.z * a;
            sh[i].a = tint.w * a;
          }
        }
        const float ssigma = snapShadowSigma(sb.sigma) * effPr * 0.5f;
        blurCheap(sh, iw, ih, ssigma);
        const Rect shRect{{xf.origin.x + sb.offset.x, xf.origin.y + sb.offset.y}, xf.size};
        blitBuffer(dest, w, h, sh, iw, ih, shRect, 1.f);
      }
      for (std::size_t i = 0; i < card.size() && i < tmp.size(); ++i) {
        srcOver(tmp[i], card[i]);
      }
    } else {
      paintShapes(tmp, iw, ih, *local, images, localX, localClip, effPr);
    }
    blitBuffer(dest, w, h, tmp, iw, ih, xf, g.params.opacity);
    return;
  }
  paintShapes(dest, w, h, g, images, world, clip, pixelRatio);
}

void rasterIsolate(
    std::vector<Pixel>& dest,
    int w,
    int h,
    const Isolate& iso,
    const ImageStore& images,
    float pixelRatio
);

void paintBlits(
    std::vector<Pixel>& dest,
    int w,
    int h,
    const std::vector<BlitQuad>& blits,
    const ImageStore& images
) {
  for (const BlitQuad& q : blits) {
    Blit b;
    b.rect = {{q.x, q.y}, {q.w, q.h}};
    b.matter.kind = MatterKind::Sampled;
    b.matter.imageId = q.imageId;
    b.matter.uv = {{q.u0, q.v0}, {q.u1 - q.u0, q.v1 - q.v0}};
    const Blend blend = q.plus != 0 ? Blend::Plus : Blend::SrcOver;
    blitImage(dest, w, h, b, images, {q.r, q.g, q.b, q.a}, {}, q.sdf != 0, blend);
  }
}

void paintQuads(
    std::vector<Pixel>& dest,
    int w,
    int h,
    const std::vector<Quad>& quads,
    const std::vector<BlitQuad>& blits,
    const std::vector<GradientQuad>& grads,
    const std::vector<Isolate>& isolates,
    const ImageStore& images,
    float pixelRatio
) {
  for (const Quad& q : quads) {
    fillQuad(dest, w, h, q);
  }
  fillGradients(dest, w, h, grads);
  paintBlits(dest, w, h, blits, images);
  for (const Isolate& iso : isolates) {
    rasterIsolate(dest, w, h, iso, images, pixelRatio);
  }
}

void rasterIsolate(
    std::vector<Pixel>& dest,
    int w,
    int h,
    const Isolate& iso,
    const ImageStore& images,
    float pixelRatio
) {
  std::vector<Pixel> tmp(static_cast<std::size_t>(iso.contentW * iso.contentH), Pixel{0, 0, 0, 0});
  if (iso.hasGlass) {
    if (!iso.glassPills.empty()) {
      const Rect destR{{iso.destX, iso.destY}, {iso.destW, iso.destH}};
      const float pr = iso.destW > 0.f ? static_cast<float>(iso.contentW) / iso.destW : pixelRatio;
      const Rect local{{0.f, 0.f}, {iso.destW, iso.destH}};
      const PlateSpec field = glassPillsToField(
          iso.glassPills.empty() ? nullptr : iso.glassPills.data(),
          static_cast<int>(iso.glassPills.size()),
          iso.glass.mergeKPx,
          pr,
          local
      );
      sampleGlassPlate(tmp, iso.contentW, iso.contentH, dest, w, h, destR, iso.glass, field);
    }
  } else if (iso.backdropSigma > 0.f) {
    const Rect destR{{iso.destX, iso.destY}, {iso.destW, iso.destH}};
    const float pr = iso.destW > 0.f ? static_cast<float>(iso.contentW) / iso.destW : pixelRatio;
    PlateSpec copy = specFromIsolate(iso, pr);
    copyDestPlate(tmp, iso.contentW, iso.contentH, dest, w, h, destR);
    blurCheap(tmp, iso.contentW, iso.contentH, iso.backdropSigma * pixelRatio * 0.5f);
    maskFrostPlate(tmp, iso.contentW, iso.contentH, copy);
  }
  if (iso.hasShadow || iso.contentSigma > 0.f) {
    std::vector<Pixel> card(
        static_cast<std::size_t>(iso.contentW * iso.contentH), Pixel{0, 0, 0, 0}
    );
    paintQuads(
        card,
        iso.contentW,
        iso.contentH,
        iso.quads,
        iso.blits,
        iso.gradients,
        iso.isolates,
        images,
        pixelRatio
    );
    if (iso.contentSigma > 0.f) {
      blurCheap(card, iso.contentW, iso.contentH, iso.contentSigma * pixelRatio * 0.5f);
    }
    if (iso.hasShadow) {
      const Vec4 tint = premulLinear(iso.shadow.color);
      std::vector<Pixel> sh(
          static_cast<std::size_t>(iso.contentW * iso.contentH), Pixel{0, 0, 0, 0}
      );
      for (std::size_t i = 0; i < card.size() && i < sh.size(); ++i) {
        const float a = card[i].a;
        if (a > 1e-5f) {
          sh[i].r = tint.x * a;
          sh[i].g = tint.y * a;
          sh[i].b = tint.z * a;
          sh[i].a = tint.w * a;
        }
      }
      blurCheap(sh, iso.contentW, iso.contentH, iso.shadow.sigma * pixelRatio * 0.5f);
      const Rect shRect{
          {iso.destX + iso.shadow.offset.x, iso.destY + iso.shadow.offset.y}, {iso.destW, iso.destH}
      };
      blitBuffer(dest, w, h, sh, iso.contentW, iso.contentH, shRect, 1.f);
    }
    for (std::size_t i = 0; i < card.size() && i < tmp.size(); ++i) {
      srcOver(tmp[i], card[i]);
    }
  } else {
    paintQuads(
        tmp,
        iso.contentW,
        iso.contentH,
        iso.quads,
        iso.blits,
        iso.gradients,
        iso.isolates,
        images,
        pixelRatio
    );
  }
  blitBuffer(
      dest,
      w,
      h,
      tmp,
      iso.contentW,
      iso.contentH,
      Rect{{iso.destX, iso.destY}, {iso.destW, iso.destH}},
      iso.opacity
  );
}

void toRgba(const std::vector<Pixel>& buf, std::uint8_t* rgba) {
  for (std::size_t i = 0; i < buf.size(); ++i) {
    // Working buffer is linear; encode once at the edge.
    rgba[i * 4 + 0] = static_cast<std::uint8_t>(std::round(linearToSrgb(buf[i].r) * 255.f));
    rgba[i * 4 + 1] = static_cast<std::uint8_t>(std::round(linearToSrgb(buf[i].g) * 255.f));
    rgba[i * 4 + 2] = static_cast<std::uint8_t>(std::round(linearToSrgb(buf[i].b) * 255.f));
    rgba[i * 4 + 3] = static_cast<std::uint8_t>(std::round(std::clamp(buf[i].a, 0.f, 1.f) * 255.f));
  }
}

}  // namespace

void rasterScene(
    const Scene& scene, int width, int height, std::uint8_t* rgba, RasterOptions options
) {
  tlsRaster = options;
  thread_local std::vector<Pixel> buf;
  buf.assign(static_cast<std::size_t>(width * height), Pixel{0, 0, 0, 1});
  Group root = merge(scene.root, nullptr);
  const float pr =
      scene.logicalSize.x > 0.f ? static_cast<float>(width) / scene.logicalSize.x : 1.f;
  rasterGroup(buf, width, height, root, scene.images, Mat4::scale(pr, pr), {}, pr);
  toRgba(buf, rgba);
}

void rasterPacket(
    const FramePacket& packet, int width, int height, std::uint8_t* rgba, RasterOptions options
) {
  tlsRaster = options;
  thread_local std::vector<Pixel> buf;
  buf.assign(static_cast<std::size_t>(width * height), Pixel{0, 0, 0, 1});
  const float pr =
      packet.logicalSize.x > 0.f ? static_cast<float>(width) / packet.logicalSize.x : 1.f;
  paintQuads(
      buf,
      width,
      height,
      packet.quads,
      packet.blits,
      packet.gradients,
      packet.isolates,
      packet.images,
      pr
  );
  toRgba(buf, rgba);
}

}  // namespace glim::paint
