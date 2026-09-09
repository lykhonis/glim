#include <glim/paint/FramePacket.h>

#include <algorithm>
#include <cmath>

namespace glim::paint {
namespace {

Quad toQuad(const FillRect& f) {
    const Vec4 p = f.color.premul();
    Quad q;
    q.x = f.rect.origin.x;
    q.y = f.rect.origin.y;
    q.w = f.rect.size.x;
    q.h = f.rect.size.y;
    q.r = p.x;
    q.g = p.y;
    q.b = p.z;
    q.a = p.w;
    return q;
}

void appendShapes(std::vector<Quad>& out, const Group& g, const Mat4& extra) {
    for (const Shape& s : g.shapes) {
        if (const auto* f = std::get_if<FillRect>(&s)) {
            out.push_back(toQuad(transformFill(extra, *f)));
        }
    }
}

Isolate encodeIsolate(const Group& g) {
    Isolate iso;
    iso.opacity = g.params.opacity;
    const Rect b = g.params.bounds.size.x > 0 ? g.params.bounds : contentBounds(g);
    iso.contentW = std::max(1, static_cast<int>(std::ceil(b.size.x)));
    iso.contentH = std::max(1, static_cast<int>(std::ceil(b.size.y)));
    const FillRect dest = transformFill(g.params.transform, FillRect{Rect{{0, 0}, b.size}, Color{}});
    iso.destX = dest.rect.origin.x;
    iso.destY = dest.rect.origin.y;
    iso.destW = dest.rect.size.x;
    iso.destH = dest.rect.size.y;
    appendShapes(iso.quads, g, Mat4::identity());
    for (const auto& child : g.children) {
        if (!child) {
            continue;
        }
        if (needsIsolate(*child)) {
            iso.isolates.push_back(encodeIsolate(*child));
        } else {
            appendShapes(iso.quads, *child, child->params.transform);
        }
    }
    return iso;
}

void encodeMerged(FramePacket& packet, const Group& g) {
    appendShapes(packet.quads, g, Mat4::identity());
    for (const auto& child : g.children) {
        if (!child) {
            continue;
        }
        if (needsIsolate(*child)) {
            packet.isolates.push_back(encodeIsolate(*child));
            ++packet.stats.isolateCount;
        } else {
            appendShapes(packet.quads, *child, child->params.transform);
        }
    }
}

}  // namespace

FramePacket encode(const Scene& scene, float pixelRatio) {
    FramePacket packet;
    packet.logicalSize = scene.logicalSize;
    Group root = merge(scene.root, &packet.stats);
    encodeMerged(packet, root);
    packet.stats.instances = static_cast<unsigned>(packet.quads.size());
    packet.stats.tileCount = static_cast<unsigned>(coarseTileCount(scene.logicalSize, pixelRatio));
    return packet;
}

}  // namespace glim::paint
