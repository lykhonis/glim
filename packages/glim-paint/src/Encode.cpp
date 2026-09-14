#include <glim/paint/FramePacket.h>

#include <algorithm>
#include <cmath>

namespace glim::paint {
namespace {

Quad toQuad(const FillRect& f) {
    const Vec4 p = f.matter.color.premul();
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

BlitQuad toBlitQuad(const Blit& b) {
    const Vec4 p = b.matter.color.premul();
    BlitQuad q;
    q.x = b.rect.origin.x;
    q.y = b.rect.origin.y;
    q.w = b.rect.size.x;
    q.h = b.rect.size.y;
    q.u0 = b.matter.uv.origin.x;
    q.v0 = b.matter.uv.origin.y;
    q.u1 = b.matter.uv.origin.x + b.matter.uv.size.x;
    q.v1 = b.matter.uv.origin.y + b.matter.uv.size.y;
    q.r = p.x;
    q.g = p.y;
    q.b = p.z;
    q.a = p.w;
    q.imageId = b.matter.imageId;
    return q;
}

void appendShapes(std::vector<Quad>& quads, std::vector<BlitQuad>& blits, const Group& g,
                  const Mat4& extra) {
    for (const Shape& s : g.shapes) {
        if (const auto* f = std::get_if<FillRect>(&s)) {
            quads.push_back(toQuad(transformFill(extra, *f)));
        } else if (const auto* b = std::get_if<Blit>(&s)) {
            blits.push_back(toBlitQuad(transformBlit(extra, *b)));
        }
    }
}

Isolate encodeIsolate(const Group& g) {
    Isolate iso;
    iso.opacity = g.params.opacity;
    const Rect b = g.params.bounds.size.x > 0 ? g.params.bounds : contentBounds(g);
    iso.contentW = std::max(1, static_cast<int>(std::ceil(b.size.x)));
    iso.contentH = std::max(1, static_cast<int>(std::ceil(b.size.y)));
    const Rect dest = transformRect(g.params.transform, Rect{{0, 0}, b.size});
    iso.destX = dest.origin.x;
    iso.destY = dest.origin.y;
    iso.destW = dest.size.x;
    iso.destH = dest.size.y;
    appendShapes(iso.quads, iso.blits, g, Mat4::identity());
    for (const auto& child : g.children) {
        if (!child) {
            continue;
        }
        if (needsIsolate(*child)) {
            iso.isolates.push_back(encodeIsolate(*child));
        } else {
            appendShapes(iso.quads, iso.blits, *child, child->params.transform);
        }
    }
    return iso;
}

void encodeMerged(FramePacket& packet, const Group& g) {
    appendShapes(packet.quads, packet.blits, g, Mat4::identity());
    for (const auto& child : g.children) {
        if (!child) {
            continue;
        }
        if (needsIsolate(*child)) {
            packet.isolates.push_back(encodeIsolate(*child));
            ++packet.stats.isolateCount;
        } else {
            appendShapes(packet.quads, packet.blits, *child, child->params.transform);
        }
    }
}

}  // namespace

FramePacket encode(const Scene& scene, float pixelRatio) {
    FramePacket packet;
    packet.logicalSize = scene.logicalSize;
    packet.images = scene.images;
    Group root = merge(scene.root, &packet.stats);
    encodeMerged(packet, root);
    packet.stats.instances =
        static_cast<unsigned>(packet.quads.size() + packet.blits.size());
    packet.stats.tileCount = static_cast<unsigned>(coarseTileCount(scene.logicalSize, pixelRatio));
    return packet;
}

}  // namespace glim::paint
