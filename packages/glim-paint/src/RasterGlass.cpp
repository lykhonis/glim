#include "RasterGlass.h"

#include <glim/paint/ColorSpace.h>

#include <algorithm>
#include <cmath>

#include "LensMath.h"

namespace glim::paint::raster {

namespace {

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

void waterRipple(float px, float py, float iw, float ih, const Lens& mat, float& rx, float& ry) {
  rx = 0.f;
  ry = 0.f;
  if (!lensRipples(mat)) {
    return;
  }
  constexpr float kTau = 6.28318f;
  const float ph1 = kTau * (px / iw * 3.f + py / ih * 2.f + mat.timeSec * 0.12f);
  const float ph2 = kTau * (px / iw * 5.f - py / ih * 3.f - mat.timeSec * 0.09f);
  rx = mat.rippleAmp * (std::cos(ph1) + 0.6f * std::cos(ph2));
  ry = mat.rippleAmp * (std::sin(ph1 * 1.3f) + 0.6f * std::sin(ph2 * 1.1f));
}

// The material's swell at plate pixel (px, py), in dome-radius units.
lens::Swell plateSwell(const Lens& mat, float px, float py, float dome) {
  return lens::swellAt(
      px / dome * mat.swellScale.x, py / dome * mat.swellScale.y, lensSwellPhase(mat)
  );
}

// Dome normal folded by the swell + mirror reflection vector for the
// opaque liquid-metal plate. tilt: the unfolded dome tilt (1 at the rim).
struct MetalNormal {
  float nx, ny, nz, rx, ry, tilt;
};

MetalNormal metalNormal(const GlassSdf& f, const Lens& mat, float px, float py) {
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
Pixel metalF0(const Lens& mat) {
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
Pixel metalEnvGlass(const GlassSdf& f, const Lens& mat, float envL, float px, float py) {
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
    Pixel p, const GlassSdf& f, const Lens& mat, float mask, float px, float py, float iw, float ih
) {
  const bool flatten = mat.flatten || mat.variant == LensVariant::Identity;
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
    const bool isGlass = mat.material == LensMaterial::Glass;
    const bool isMetal = mat.material == LensMaterial::LiquidMetal;
    const bool isWater = mat.material == LensMaterial::Water;
    const bool regular = mat.variant == LensVariant::Regular;
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
      if (hasEdge(mat.edge, LensEdge::Left)) {
        const float w = std::clamp(-f.nx, 0.f, 1.f);
        side += w * w;
      }
      if (hasEdge(mat.edge, LensEdge::Right)) {
        const float w = std::clamp(f.nx, 0.f, 1.f);
        side += w * w;
      }
      p.r *= 1.f - side * hair2 * 0.38f;
      p.g *= 1.f - side * hair2 * 0.38f;
      p.b *= 1.f - side * hair2 * 0.38f;
      const float top = hasEdge(mat.edge, LensEdge::Top) ? std::clamp(-f.ny, 0.f, 1.f) : 0.f;
      const float bot = hasEdge(mat.edge, LensEdge::Bottom) ? std::clamp(f.ny, 0.f, 1.f) : 0.f;
      const float hi = hair2 * (top * 0.35f + bot * 0.22f) * 0.9f;
      p.r = p.r + (1.f - p.r) * hi;
      p.g = p.g + (1.f - p.g) * hi;
      p.b = p.b + (1.f - p.b) * hi;
      // The soft all-around rim belongs to the full jeweled plate;
      // partial masks carry their look in the directional terms above.
      if (mat.edge == LensEdge::Full) {
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
        if (hasEdge(mat.edge, LensEdge::Left)) edgeSpec += std::clamp(-f.nx, 0.f, 1.f);
        if (hasEdge(mat.edge, LensEdge::Right)) edgeSpec += std::clamp(f.nx, 0.f, 1.f);
        if (hasEdge(mat.edge, LensEdge::Top)) edgeSpec += std::clamp(-f.ny, 0.f, 1.f);
        if (hasEdge(mat.edge, LensEdge::Bottom)) edgeSpec += std::clamp(f.ny, 0.f, 1.f);
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

}  // namespace

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

namespace {

void applyGlassChrome(
    std::vector<Pixel>& plate, int iw, int ih, const PlateSpec& field, const Lens& mat
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

}  // namespace

PlateSpec glassPillsToField(
    const LensPill* pills,
    int n,
    float mergeKPx,
    float pr,
    const Rect& surface,
    const RasterOptions& options
) {
  PlateSpec spec;
  spec.mergeK = options.glassMerge ? mergeKPx * pr : 0.f;
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

namespace {

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

}  // namespace

void sampleGlassPlate(
    std::vector<Pixel>& plate,
    int iw,
    int ih,
    const std::vector<Pixel>& dest,
    int dw,
    int dh,
    Rect src,
    const Lens& mat,
    const PlateSpec& field,
    const RasterOptions& options
) {
  if (iw <= 0 || ih <= 0 || dw <= 0 || dh <= 0 || src.size.x <= 0.f || src.size.y <= 0.f) {
    return;
  }
  const bool flatten = mat.flatten || mat.variant == LensVariant::Identity;
  const bool metal = !flatten && lensOpaque(mat);
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
  if (options.glassBlur && !flatten) {
    copyDestPlate(plate, iw, ih, dest, dw, dh, src);
    const float sigma =
        mat.material == LensMaterial::Water ? 1.f : (mat.variant == LensVariant::Clear ? 0.f : 6.f);
    blurCheap(plate, iw, ih, sigma);
    applyGlassChrome(plate, iw, ih, field, mat);
    return;
  }
  const float plateW = static_cast<float>(iw);
  const float plateH = static_cast<float>(ih);
  const float frostR = flatten ? 0.f : (mat.variant == LensVariant::Regular ? 3.25f : 0.f);
  const bool aligned = src.origin.x == static_cast<float>(static_cast<int>(src.origin.x)) &&
                       src.origin.y == static_cast<float>(static_cast<int>(src.origin.y)) &&
                       src.size.x == plateW && src.size.y == plateH;
  const int ox = static_cast<int>(src.origin.x);
  const int oy = static_cast<int>(src.origin.y);
  const bool regular = mat.variant == LensVariant::Regular;
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
        lensThicknessFor(mat, Rect{{x0 * ipr, y0 * ipr}, {(x1 - x0) * ipr, (y1 - y0) * ipr}}) *
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
      if (!flatten && mat.material == LensMaterial::Water) {
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
      if (lensRipples(mat)) {
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

}  // namespace glim::paint::raster
