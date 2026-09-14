#include <glim/paint/FramePacket.h>

#include "Strip.h"

#include <glim/assert.h>

#include <algorithm>
#include <cmath>

namespace glim::paint {
namespace {

void encodeTree(std::vector<Quad>& quads, std::vector<BlitQuad>& blits, std::vector<Isolate>& isolates,
                const Group& g, const Mat4& extra, const ClipState& parentClip, Stats* stats,
                float pixelRatio, Vec2 logicalSize);

Isolate encodeIsolate(const Group& g, const Mat4& extra, float pixelRatio, Vec2 logicalSize) {
    Isolate iso;
    iso.opacity = g.params.opacity;
    const Rect surface = hasBackdrop(g) ? backdropSurface(g)
                                        : (g.params.bounds.size.x > 0 ? g.params.bounds : contentBounds(g));
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

    Group local = g;
    local.params.opacity = 1.f;
    local.params.isolate = false;
    clearBackdropParams(local.params);
    local.params.transform = Mat4::identity();
    if (hasBackdrop(g)) {
        dropBackdropPills(local);
    }
    if (!hasClip(local.params) && surface.size.x > 0.f && surface.size.y > 0.f) {
        local.params.clip = Rect{{0, 0}, surface.size};
        local.params.clipRadius = {};
    }
    encodeTree(iso.quads, iso.blits, iso.isolates, local,
               Mat4::translate(-surface.origin.x, -surface.origin.y), {}, nullptr, pixelRatio, surface.size);
    return iso;
}

void encodeTree(std::vector<Quad>& quads, std::vector<BlitQuad>& blits, std::vector<Isolate>& isolates,
                const Group& g, const Mat4& extra, const ClipState& parentClip, Stats* stats,
                float pixelRatio, Vec2 logicalSize) {
    const ClipState clip = intersectClip(parentClip, clipOf(g.params, extra));
    bool seenBackdrop = false;
    bool afterBackdrop = false;
    visitGroup(
        g,
        [&](const Shape& s) {
            if (seenBackdrop) {
                Group wrap;
                wrap.shapes.push_back(transformShape(extra, s));
                wrap.order.push_back({GroupItem::Shape, 0});
                wrap.params.bounds = contentBounds(wrap);
                isolates.push_back(encodeIsolate(wrap, Mat4::identity(), pixelRatio, logicalSize));
                if (stats) {
                    ++stats->isolateCount;
                }
                return;
            }
            appendShape(quads, blits, transformShape(extra, s), clip);
        },
        [&](const Group& child) {
            const bool backdrop = hasBackdrop(child);
            if (backdrop) {
                GLIM_ASSERT(!afterBackdrop,
                            "backdrop Groups must be consecutive ([under*][backdrop*][overlay*])");
                seenBackdrop = true;
            } else if (seenBackdrop) {
                afterBackdrop = true;
            }
            if (needsIsolate(child) || seenBackdrop) {
                isolates.push_back(encodeIsolate(child, extra, pixelRatio, logicalSize));
                if (stats) {
                    ++stats->isolateCount;
                    if (backdrop && stats->backdropCount == 0) {
                        stats->backdropCount = 1;
                    }
                }
            } else {
                encodeTree(quads, blits, isolates, child, extra * child.params.transform, clip, stats,
                           pixelRatio, logicalSize);
            }
        });
}

}  // namespace

FramePacket encode(const Scene& scene, float pixelRatio) {
    FramePacket packet;
    packet.logicalSize = scene.logicalSize;
    packet.images = scene.images;
    Group root = merge(scene.root, &packet.stats);
    encodeTree(packet.quads, packet.blits, packet.isolates, root, Mat4::identity(), {}, &packet.stats,
               pixelRatio, scene.logicalSize);
    packet.stats.instances =
        static_cast<unsigned>(packet.quads.size() + packet.blits.size());
    packet.stats.tileCount = static_cast<unsigned>(coarseTileCount(scene.logicalSize, pixelRatio));
    return packet;
}

}  // namespace glim::paint
