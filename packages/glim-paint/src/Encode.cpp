#include <glim/paint/FramePacket.h>

#include "Strip.h"

#include <algorithm>
#include <cmath>

namespace glim::paint {
namespace {

void encodeTree(std::vector<Quad>& quads, std::vector<BlitQuad>& blits, std::vector<Isolate>& isolates,
                const Group& g, const Mat4& extra, const ClipState& parentClip, Stats* stats);

Isolate encodeIsolate(const Group& g, const Mat4& extra) {
    Isolate iso;
    iso.opacity = g.params.opacity;
    const Rect b = g.params.bounds.size.x > 0 ? g.params.bounds : contentBounds(g);
    iso.contentW = std::max(1, static_cast<int>(std::ceil(b.size.x)));
    iso.contentH = std::max(1, static_cast<int>(std::ceil(b.size.y)));
    const Rect dest = transformRect(extra * g.params.transform, Rect{{0, 0}, b.size});
    iso.destX = dest.origin.x;
    iso.destY = dest.origin.y;
    iso.destW = dest.size.x;
    iso.destH = dest.size.y;

    Group local = g;
    local.params.opacity = 1.f;
    local.params.isolate = false;
    local.params.transform = Mat4::identity();
    if (!hasClip(local.params) && b.size.x > 0.f && b.size.y > 0.f) {
        local.params.clip = Rect{{0, 0}, b.size};
        local.params.clipRadius = {};
    }
    encodeTree(iso.quads, iso.blits, iso.isolates, local, Mat4::identity(), {}, nullptr);
    return iso;
}

void encodeTree(std::vector<Quad>& quads, std::vector<BlitQuad>& blits, std::vector<Isolate>& isolates,
                const Group& g, const Mat4& extra, const ClipState& parentClip, Stats* stats) {
    const ClipState clip = intersectClip(parentClip, clipOf(g.params, extra));
    for (const Shape& s : g.shapes) {
        appendShape(quads, blits, transformShape(extra, s), clip);
    }
    for (const auto& child : g.children) {
        if (!child) {
            continue;
        }
        if (needsIsolate(*child)) {
            isolates.push_back(encodeIsolate(*child, extra));
            if (stats) {
                ++stats->isolateCount;
            }
        } else {
            encodeTree(quads, blits, isolates, *child, extra * child->params.transform, clip, stats);
        }
    }
}

}  // namespace

FramePacket encode(const Scene& scene, float pixelRatio) {
    FramePacket packet;
    packet.logicalSize = scene.logicalSize;
    packet.images = scene.images;
    Group root = merge(scene.root, &packet.stats);
    encodeTree(packet.quads, packet.blits, packet.isolates, root, Mat4::identity(), {}, &packet.stats);
    packet.stats.instances =
        static_cast<unsigned>(packet.quads.size() + packet.blits.size());
    packet.stats.tileCount = static_cast<unsigned>(coarseTileCount(scene.logicalSize, pixelRatio));
    return packet;
}

}  // namespace glim::paint
