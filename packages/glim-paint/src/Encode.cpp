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
    const Rect b = g.params.bounds.size.x > 0 ? g.params.bounds : contentBounds(g);
    iso.contentW = isolatePixelSize(b.size.x, pixelRatio);
    iso.contentH = isolatePixelSize(b.size.y, pixelRatio);
    const Rect dest = transformRect(extra * g.params.transform, b);
    iso.destX = dest.origin.x;
    iso.destY = dest.origin.y;
    iso.destW = dest.size.x;
    iso.destH = dest.size.y;
    iso.backdropSigma = snapBackdropSigma(g.params.backdropBlur);
    iso.backdropBend = g.params.backdropBend;
    if (iso.backdropSigma > 0.f && logicalSize.x > 0.f && logicalSize.y > 0.f) {
        iso.backdropU0 = dest.origin.x / logicalSize.x;
        iso.backdropV0 = dest.origin.y / logicalSize.y;
        iso.backdropU1 = (dest.origin.x + dest.size.x) / logicalSize.x;
        iso.backdropV1 = (dest.origin.y + dest.size.y) / logicalSize.y;
    }

    Group local = g;
    local.params.opacity = 1.f;
    local.params.isolate = false;
    local.params.backdropBlur = 0.f;
    local.params.backdropBend = 0.f;
    local.params.transform = Mat4::identity();
    if (!hasClip(local.params) && b.size.x > 0.f && b.size.y > 0.f) {
        local.params.clip = Rect{{0, 0}, b.size};
        local.params.clipRadius = {};
    }
    encodeTree(iso.quads, iso.blits, iso.isolates, local, Mat4::translate(-b.origin.x, -b.origin.y), {},
               nullptr, pixelRatio, b.size);
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
