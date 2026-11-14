#include <glim/assert.h>
#include <glim/paint/FramePacket.h>

#include <algorithm>
#include <chrono>
#include <cmath>

#include "GroupOrder.h"
#include "Strip.h"

namespace glim::paint {
namespace {

void encodeTree(
  std::vector<Quad>& quads,
  std::vector<BlitQuad>& blits,
  std::vector<GradientQuad>& gradients,
  std::vector<Isolate>& isolates,
  const Group& g,
  const Mat4& extra,
  const ClipState& parentClip,
  Stats* stats,
  float pixelRatio,
  Vec2 logicalSize
);

Isolate encodeIsolate(
  const Group& g, const Mat4& extra, float pixelRatio, Vec2 logicalSize, Stats* stats
) {
  Isolate iso;
  iso.opacity = g.params.opacity;
  const bool glassWork = hasGlassWork(g);
  const bool shadow = hasShadow(g);
  const bool content = hasContentBlur(g);
  GLIM_ASSERT(!(hasBackdrop(g) && glassWork), "a Group cannot be both backdrop and glass");
  const bool explicitBounds = g.params.bounds.size.x > 0.f && g.params.bounds.size.y > 0.f;
  const Rect surface =
    glassWork ? glassSurface(g)
              : (hasBackdrop(g)
                   ? backdropSurface(g)
                   : ((shadow || content) ? shadowContentSurface(g)
                                          : (explicitBounds ? g.params.bounds : contentBounds(g))));
  iso.contentW = isolatePixelSizeFor(g, surface.size.x, pixelRatio);
  iso.contentH = isolatePixelSizeFor(g, surface.size.y, pixelRatio);
  const Rect dest = transformRect(extra * g.params.transform, surface);
  iso.destX = dest.origin.x;
  iso.destY = dest.origin.y;
  iso.destW = dest.size.x;
  iso.destH = dest.size.y;
  iso.backdropSigma = snapBackdropSigma(g.params.backdropBlur);
  iso.backdropRadius = plateRadius(g.params.clipRadius);
  iso.backdropFlat = g.params.backdropFlat;
  if (hasBackdrop(g)) {
    BackdropPill pills[kMaxBackdropPills];
    const int n = collectBackdropPills(g, pills);
    iso.backdropPills.assign(pills, pills + n);
    for (BackdropPill& pill : iso.backdropPills) {
      pill.rect.origin.x -= surface.origin.x;
      pill.rect.origin.y -= surface.origin.y;
    }
  }
  if (iso.backdropSigma > 0.f && logicalSize.x > 0.f && logicalSize.y > 0.f) {
    iso.backdropU0 = dest.origin.x / logicalSize.x;
    iso.backdropV0 = dest.origin.y / logicalSize.y;
    iso.backdropU1 = (dest.origin.x + dest.size.x) / logicalSize.x;
    iso.backdropV1 = (dest.origin.y + dest.size.y) / logicalSize.y;
  }
  iso.hasShadow = shadow;
  if (shadow && g.params.shadow.has_value()) {
    iso.shadow = *g.params.shadow;
    iso.shadow.sigma = snapShadowSigma(iso.shadow.sigma);
  }
  iso.contentSigma = snapContentSigma(g.params.contentBlur);
  if (stats) {
    if (shadow) {
      ++stats->shadowPassCount;
    }
    if (content) {
      ++stats->contentBlurCount;
    }
  }
  if (glassWork) {
    iso.hasGlass = hasGlass(g) || isGlassContainer(g);
    iso.glassContainer = isGlassContainer(g);
    iso.lens = lensOf(g);
    LensPill pills[kMaxLensPills];
    const int n = collectLensPills(g, pills);
    iso.lensPills.assign(pills, pills + n);
    for (LensPill& pill : iso.lensPills) {
      pill.rect.origin.x -= surface.origin.x;
      pill.rect.origin.y -= surface.origin.y;
    }
    if (logicalSize.x > 0.f && logicalSize.y > 0.f) {
      iso.glassU0 = dest.origin.x / logicalSize.x;
      iso.glassV0 = dest.origin.y / logicalSize.y;
      iso.glassU1 = (dest.origin.x + dest.size.x) / logicalSize.x;
      iso.glassV1 = (dest.origin.y + dest.size.y) / logicalSize.y;
    }
    if (stats) {
      ++stats->glassPassCount;
      stats->glassPillCount += static_cast<unsigned>(iso.lensPills.size());
    }
  }

  Group local = g;
  local.params.opacity = 1.f;
  local.params.isolate = false;
  clearBackdropParams(local.params);
  clearShadowParams(local.params);
  local.params.transform = Mat4::identity();
  if (hasBackdrop(g)) {
    dropBackdropPills(local);
  }
  if (glassWork) {
    stripGlassForIsolate(local);
  }
  if (!hasClip(local.params) && surface.size.x > 0.f && surface.size.y > 0.f) {
    local.params.clip = surface;
    local.params.clipRadius = {};
  }
  encodeTree(
    iso.quads,
    iso.blits,
    iso.gradients,
    iso.isolates,
    local,
    Mat4::translate(-surface.origin.x, -surface.origin.y),
    {},
    nullptr,
    pixelRatio,
    surface.size
  );
  // Nested isolates encode against surface.size with relative UVs, not logicalSize.
  return iso;
}

void markPlus(
  std::vector<Quad>& quads,
  std::vector<BlitQuad>& blits,
  std::vector<GradientQuad>& grads,
  std::size_t nq,
  std::size_t nb,
  std::size_t ng
) {
  for (std::size_t i = nq; i < quads.size(); ++i) {
    quads[i].plus = 1;
  }
  for (std::size_t i = nb; i < blits.size(); ++i) {
    blits[i].plus = 1;
  }
  for (std::size_t i = ng; i < grads.size(); ++i) {
    grads[i].plus = 1;
  }
}

void encodeTree(
  std::vector<Quad>& quads,
  std::vector<BlitQuad>& blits,
  std::vector<GradientQuad>& gradients,
  std::vector<Isolate>& isolates,
  const Group& g,
  const Mat4& extra,
  const ClipState& parentClip,
  Stats* stats,
  float pixelRatio,
  Vec2 logicalSize
) {
  const ClipState clip = intersectClip(parentClip, clipOf(g.params, extra));
  const bool isPlus = g.params.blend == Blend::Plus;
  GroupOrder runs;
  const auto flushWrap = [&](Group& wrap) {
    wrap.params.bounds = contentBounds(wrap);
    wrap.params.blend = g.params.blend;
    isolates.push_back(encodeIsolate(wrap, Mat4::identity(), pixelRatio, logicalSize, stats));
    if (stats) {
      ++stats->isolateCount;
    }
  };
  Group overlay;
  const auto flushOverlay = [&] {
    if (overlay.shapes.empty()) return;
    flushWrap(overlay);
    overlay = Group{};
  };
  visitGroup(
    g,
    [&](const Shape& s) {
      if (runs.seenBackdrop || runs.seenGlass) {
        // Plus parents add each shape on its own; normal-blend runs share
        // one isolate (src-over is associative), so overlay text and
        // strokes do not pay an offscreen each.
        if (isPlus) {
          Group wrap;
          wrap.shapes.push_back(transformShape(extra, s));
          wrap.order.push_back({GroupItem::Shape, 0});
          flushWrap(wrap);
          return;
        }
        overlay.order.push_back(
          {GroupItem::Shape, static_cast<std::uint32_t>(overlay.shapes.size())}
        );
        overlay.shapes.push_back(transformShape(extra, s));
        return;
      }
      const std::size_t nq = quads.size();
      const std::size_t nb = blits.size();
      const std::size_t ng = gradients.size();
      const auto t0 =
        stats ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
      appendShapeWorld(quads, blits, gradients, s, extra, clip);
      if (stats) {
        const std::size_t k = s.index();
        ++stats->shapeCount[k];
        stats->shapeInstances[k] +=
          static_cast<unsigned>(quads.size() + blits.size() + gradients.size() - nq - nb - ng);
        stats->shapeEncodeMs[k] +=
          std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
      }
      if (isPlus) {
        markPlus(quads, blits, gradients, nq, nb, ng);
      }
    },
    [&](const Group& child) {
      flushOverlay();
      const bool backdrop = hasBackdrop(child);
      const bool glassWork = hasGlassWork(child);
      runs.next(backdrop, glassWork);
      if (needsIsolate(child) || runs.seenBackdrop || runs.seenGlass) {
        isolates.push_back(encodeIsolate(child, extra, pixelRatio, logicalSize, stats));
        if (stats) {
          ++stats->isolateCount;
          if (backdrop && stats->backdropCount == 0) {
            stats->backdropCount = 1;
          }
        }
      } else {
        encodeTree(
          quads,
          blits,
          gradients,
          isolates,
          child,
          extra * child.params.transform,
          clip,
          stats,
          pixelRatio,
          logicalSize
        );
      }
    }
  );
  flushOverlay();
}

}  // namespace

FramePacket encode(const Scene& scene, float pixelRatio) {
  FramePacket packet;
  packet.logicalSize = scene.logicalSize;
  packet.images = scene.images;
  Group root = merge(scene.root, &packet.stats);
  encodeTree(
    packet.quads,
    packet.blits,
    packet.gradients,
    packet.isolates,
    root,
    Mat4::identity(),
    {},
    &packet.stats,
    pixelRatio,
    scene.logicalSize
  );
  packet.stats.instances =
    static_cast<unsigned>(packet.quads.size() + packet.blits.size() + packet.gradients.size());
  packet.stats.tileCount = static_cast<unsigned>(coarseTileCount(scene.logicalSize, pixelRatio));
  packet.stats.dirtyTiles = packet.stats.tileCount;
  packet.dirtyRect = {{0.f, 0.f}, scene.logicalSize};
  return packet;
}

}  // namespace glim::paint
