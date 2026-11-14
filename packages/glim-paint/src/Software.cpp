#include <glim/assert.h>
#include <glim/paint/ColorSpace.h>
#include <glim/paint/Software.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "GroupOrder.h"
#include "Parallel.h"
#include "RasterFill.h"
#include "RasterGlass.h"
#include "RasterPixel.h"
#include "RasterPlate.h"
#include "Strip.h"

namespace glim::paint {

thread_local RasterOptions tlsRaster{};

namespace raster {
namespace {

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
      glassWork ? glassSurface(g)
                : (hasBackdrop(g) ? backdropSurface(g)
                                  : ((shadow || content)
                                       ? shadowContentSurface(g)
                                       : (explicitBounds ? g.params.bounds : contentBounds(g))));
    const float pr = pixelRatio > 0.f ? pixelRatio : 1.f;
    const float effPr = useHalfIsolate(g) ? pr * 0.5f : pr;
    const int iw = isolatePixelSize(surface.size.x, effPr);
    const int ih = isolatePixelSize(surface.size.y, effPr);
    std::vector<Pixel> tmp(static_cast<std::size_t>(iw * ih), Pixel{0, 0, 0, 0});
    const Rect xf = transformRect(world, surface);
    if (glassWork) {
      const Lens& mat = lensOf(g);
      LensPill pills[kMaxLensPills];
      const int n = collectLensPills(g, pills);
      if (n > 0) {
        const PlateSpec field = glassPillsToField(pills, n, mat.mergeKPx, pr, surface, tlsRaster);
        sampleGlassPlate(tmp, iw, ih, dest, w, h, xf, mat, field, tlsRaster);
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
    if (!iso.lensPills.empty()) {
      const Rect destR{{iso.destX, iso.destY}, {iso.destW, iso.destH}};
      const float pr = iso.destW > 0.f ? static_cast<float>(iso.contentW) / iso.destW : pixelRatio;
      const Rect local{{0.f, 0.f}, {iso.destW, iso.destH}};
      const PlateSpec field = glassPillsToField(
        iso.lensPills.empty() ? nullptr : iso.lensPills.data(),
        static_cast<int>(iso.lensPills.size()),
        iso.lens.mergeKPx,
        pr,
        local,
        tlsRaster
      );
      sampleGlassPlate(
        tmp, iso.contentW, iso.contentH, dest, w, h, destR, iso.lens, field, tlsRaster
      );
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
}  // namespace raster

void rasterScene(
  const Scene& scene, int width, int height, std::uint8_t* rgba, RasterOptions options
) {
  tlsRaster = options;
  thread_local std::vector<raster::Pixel> buf;
  buf.assign(static_cast<std::size_t>(width * height), raster::Pixel{0, 0, 0, 1});
  Group root = merge(scene.root, nullptr);
  const float pr =
    scene.logicalSize.x > 0.f ? static_cast<float>(width) / scene.logicalSize.x : 1.f;
  raster::rasterGroup(buf, width, height, root, scene.images, Mat4::scale(pr, pr), {}, pr);
  raster::toRgba(buf, rgba);
}

void rasterPacket(
  const FramePacket& packet, int width, int height, std::uint8_t* rgba, RasterOptions options
) {
  tlsRaster = options;
  thread_local std::vector<raster::Pixel> buf;
  buf.assign(static_cast<std::size_t>(width * height), raster::Pixel{0, 0, 0, 1});
  const float pr =
    packet.logicalSize.x > 0.f ? static_cast<float>(width) / packet.logicalSize.x : 1.f;
  raster::paintQuads(
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
  raster::toRgba(buf, rgba);
}

}  // namespace glim::paint
