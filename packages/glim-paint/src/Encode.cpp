#include <glim/paint/FramePacket.h>

#include "Strip.h"

#include <glim/assert.h>

#include <algorithm>
#include <cmath>

namespace glim::paint {
namespace {

void encodeTree(std::vector<Quad>& quads, std::vector<BlitQuad>& blits,
                std::vector<GradientQuad>& gradients, std::vector<Isolate>& isolates, const Group& g,
                const Mat4& extra, const ClipState& parentClip, Stats* stats, float pixelRatio,
                Vec2 logicalSize);

Isolate encodeIsolate(const Group& g, const Mat4& extra, float pixelRatio, Vec2 logicalSize, Stats* stats) {
    Isolate iso;
    iso.opacity = g.params.opacity;
    const bool glassWork = hasGlassWork(g);
    const bool shadow = hasShadow(g);
    const bool content = hasContentBlur(g);
    GLIM_ASSERT(!(hasBackdrop(g) && glassWork), "a Group cannot be both backdrop and glass");
    const bool explicitBounds =
        g.params.bounds.size.x > 0.f && g.params.bounds.size.y > 0.f;
    const Rect surface = glassWork ? glassSurface(g)
                                   : (hasBackdrop(g) ? backdropSurface(g)
                                                     : ((shadow || content) ? shadowContentSurface(g)
                                                     : (explicitBounds ? g.params.bounds
                                                                       : contentBounds(g))));
    iso.contentW = isolatePixelSizeFor(g, surface.size.x, pixelRatio);
    iso.contentH = isolatePixelSizeFor(g, surface.size.y, pixelRatio);
    const Rect dest = transformRect(extra * g.params.transform, surface);
    iso.destX = dest.origin.x;
    iso.destY = dest.origin.y;
    iso.destW = dest.size.x;
    iso.destH = dest.size.y;
    iso.backdropSigma = snapBackdropSigma(g.params.backdropBlur);
    iso.backdropBend = g.params.backdropBend;
    iso.backdropRadius = plateRadius(g.params.clipRadius);
    iso.backdropMerge = g.params.backdropMerge;
    iso.backdropPress = g.params.backdropPress;
    iso.backdropLightX = g.params.backdropLightX;
    iso.backdropLightY = g.params.backdropLightY;
    iso.backdropLightZ = g.params.backdropLightZ;
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
    if ((iso.backdropSigma > 0.f || iso.backdropBend > 0.f) && logicalSize.x > 0.f &&
        logicalSize.y > 0.f) {
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
        if (g.params.glass.has_value()) {
            iso.glass = *g.params.glass;
        } else {
            for (const auto& c : g.children) {
                if (c && c->params.glass.has_value()) {
                    iso.glass = *c->params.glass;
                    break;
                }
            }
        }
        GlassPill pills[kMaxGlassPills];
        const int n = collectGlassPills(g, pills);
        iso.glassPills.assign(pills, pills + n);
        for (GlassPill& pill : iso.glassPills) {
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
            stats->glassPillCount += static_cast<unsigned>(iso.glassPills.size());
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
    encodeTree(iso.quads, iso.blits, iso.gradients, iso.isolates, local,
                Mat4::translate(-surface.origin.x, -surface.origin.y), {}, nullptr, pixelRatio, surface.size);
    // NOTE: nested isolates encode against surface.size, not the window. Their
    // backdrop/glass UVs are relative to this isolate's buffer, which is the
    // buffer they sample when rasterized (rasterIsolate paints nested isolates
    // into tmp). Relative addressing is correct; do not pass logicalSize here.
    return iso;
}

void markPlus(std::vector<Quad>& quads, std::vector<BlitQuad>& blits,
              std::vector<GradientQuad>& grads, std::size_t nq, std::size_t nb, std::size_t ng) {
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

void encodeTree(std::vector<Quad>& quads, std::vector<BlitQuad>& blits,
                std::vector<GradientQuad>& gradients, std::vector<Isolate>& isolates, const Group& g,
                const Mat4& extra, const ClipState& parentClip, Stats* stats, float pixelRatio,
                Vec2 logicalSize) {
    const ClipState clip = intersectClip(parentClip, clipOf(g.params, extra));
    const bool isPlus = g.params.blend == Blend::Plus;
    bool seenBackdrop = false;
    bool afterBackdrop = false;
    bool seenGlass = false;
    bool afterGlass = false;
    visitGroup(
        g,
        [&](const Shape& s) {
            if (seenBackdrop || seenGlass) {
                Group wrap;
                wrap.shapes.push_back(transformShape(extra, s));
                wrap.order.push_back({GroupItem::Shape, 0});
                wrap.params.bounds = contentBounds(wrap);
                wrap.params.blend = g.params.blend;
                isolates.push_back(encodeIsolate(wrap, Mat4::identity(), pixelRatio, logicalSize, stats));
                if (stats) {
                    ++stats->isolateCount;
                }
                return;
            }
            const std::size_t nq = quads.size();
            const std::size_t nb = blits.size();
            const std::size_t ng = gradients.size();
            appendShape(quads, blits, gradients, transformShape(extra, s), clip);
            if (isPlus) {
                markPlus(quads, blits, gradients, nq, nb, ng);
            }
        },
        [&](const Group& child) {
            const bool backdrop = hasBackdrop(child);
            const bool glassWork = hasGlassWork(child);
            if (backdrop) {
                GLIM_ASSERT(!afterBackdrop && !seenGlass,
                            "backdrop Groups must be consecutive ([under*][backdrop*][glass*][overlay*])");
                seenBackdrop = true;
            } else if (glassWork) {
                GLIM_ASSERT(!afterGlass,
                            "glass Groups must be consecutive ([under*][backdrop*][glass*][overlay*])");
                if (seenBackdrop) {
                    afterBackdrop = true;
                }
                seenGlass = true;
            } else if (seenGlass) {
                afterGlass = true;
            } else if (seenBackdrop) {
                afterBackdrop = true;
            }
            if (needsIsolate(child) || seenBackdrop || seenGlass) {
                isolates.push_back(encodeIsolate(child, extra, pixelRatio, logicalSize, stats));
                if (stats) {
                    ++stats->isolateCount;
                    if (backdrop && stats->backdropCount == 0) {
                        stats->backdropCount = 1;
                    }
                }
            } else {
                encodeTree(quads, blits, gradients, isolates, child, extra * child.params.transform,
                           clip, stats, pixelRatio, logicalSize);
            }
        });
}

}  // namespace

FramePacket encode(const Scene& scene, float pixelRatio) {
    FramePacket packet;
    packet.logicalSize = scene.logicalSize;
    packet.images = scene.images;
    Group root = merge(scene.root, &packet.stats);
    encodeTree(packet.quads, packet.blits, packet.gradients, packet.isolates, root, Mat4::identity(),
               {}, &packet.stats, pixelRatio, scene.logicalSize);
    packet.stats.instances = static_cast<unsigned>(packet.quads.size() + packet.blits.size() +
                                                   packet.gradients.size());
    packet.stats.tileCount = static_cast<unsigned>(coarseTileCount(scene.logicalSize, pixelRatio));
    packet.stats.dirtyTiles = packet.stats.tileCount;
    packet.dirtyRect = {{0.f, 0.f}, scene.logicalSize};
    return packet;
}

}  // namespace glim::paint
