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
    GLIM_ASSERT(!(hasBackdrop(g) && glassWork), "a Group cannot be both backdrop and glass");
    // If both are ever set (release build), glass wins: same precedence as the
    // Renderer path. Backdrop and glass never share one isolate.
    const bool explicitBounds =
        g.params.bounds.size.x > 0.f && g.params.bounds.size.y > 0.f;
    const Rect surface = glassWork ? glassSurface(g)
                                   : (hasBackdrop(g) ? backdropSurface(g)
                                                     : (explicitBounds ? g.params.bounds
                                                                       : contentBounds(g)));
    iso.contentW = isolatePixelSize(surface.size.x, pixelRatio);
    iso.contentH = isolatePixelSize(surface.size.y, pixelRatio);
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
    local.params.transform = Mat4::identity();
    if (hasBackdrop(g)) {
        dropBackdropPills(local);
    }
    if (glassWork) {
        stripGlassForIsolate(local);
    }
    if (!hasClip(local.params) && surface.size.x > 0.f && surface.size.y > 0.f) {
        local.params.clip = Rect{{0, 0}, surface.size};
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

void encodeTree(std::vector<Quad>& quads, std::vector<BlitQuad>& blits,
                std::vector<GradientQuad>& gradients, std::vector<Isolate>& isolates, const Group& g,
                const Mat4& extra, const ClipState& parentClip, Stats* stats, float pixelRatio,
                Vec2 logicalSize) {
    const ClipState clip = intersectClip(parentClip, clipOf(g.params, extra));
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
                isolates.push_back(encodeIsolate(wrap, Mat4::identity(), pixelRatio, logicalSize, stats));
                if (stats) {
                    ++stats->isolateCount;
                }
                return;
            }
            appendShape(quads, blits, gradients, transformShape(extra, s), clip);
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
    return packet;
}

}  // namespace glim::paint
