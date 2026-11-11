#include "RasterFill.h"

#include <glim/paint/ColorSpace.h>

#include <algorithm>
#include <cmath>

#include "Font.h"
#include "RasterPlate.h"

namespace glim::paint::raster {

namespace {

inline Blend quadBlend(const Quad& q, Blend fallback) {
  return (fallback == Blend::Plus || q.plus != 0) ? Blend::Plus : Blend::SrcOver;
}

}  // namespace

void fillQuad(std::vector<Pixel>& buf, int w, int h, const Quad& q, Blend fallback) {
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
    std::vector<Pixel>& buf, int w, int h, const std::vector<Quad>& quads, Blend fallback
) {
  for (const Quad& q : quads) {
    fillQuad(buf, w, h, q, fallback);
  }
}

namespace {

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

}  // namespace

void fillGradients(
    std::vector<Pixel>& buf, int w, int h, const std::vector<GradientQuad>& grads, Blend fallback
) {
  for (const GradientQuad& q : grads) {
    fillGradientQuad(buf, w, h, q, fallback);
  }
}

namespace {

float clipCoverageAt(const ClipState& clip, float px, float py) {
  if (!clip.active) {
    return 1.f;
  }
  if (clip.rect.size.x <= 0.f || clip.rect.size.y <= 0.f) {
    return 0.f;
  }
  return roundedCoverage(clip.rect, clip.radius, px, py);
}

}  // namespace

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

namespace {

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

}  // namespace

void blitImage(
    std::vector<Pixel>& dest,
    int w,
    int h,
    const Blit& b,
    const ImageStore& images,
    Vec4 tint,
    const ClipState& clip,
    bool sdf,
    Blend blend
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

}  // namespace glim::paint::raster
