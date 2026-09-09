#include <glim/paint/Software.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace glim::paint {
namespace {

struct Pixel {
    float r, g, b, a;
};

void srcOver(Pixel& dst, Pixel src) {
    const float ia = 1.0f - src.a;
    dst.r = src.r + dst.r * ia;
    dst.g = src.g + dst.g * ia;
    dst.b = src.b + dst.b * ia;
    dst.a = src.a + dst.a * ia;
}

void fillRect(std::vector<Pixel>& buf, int w, int h, const FillRect& f) {
    const int x0 = std::max(0, static_cast<int>(std::floor(f.rect.origin.x)));
    const int y0 = std::max(0, static_cast<int>(std::floor(f.rect.origin.y)));
    const int x1 = std::min(w, static_cast<int>(std::ceil(f.rect.origin.x + f.rect.size.x)));
    const int y1 = std::min(h, static_cast<int>(std::ceil(f.rect.origin.y + f.rect.size.y)));
    const Pixel src{f.color.premul().x, f.color.premul().y, f.color.premul().z, f.color.premul().w};
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            srcOver(buf[static_cast<std::size_t>(y * w + x)], src);
        }
    }
}

void fillQuad(std::vector<Pixel>& buf, int w, int h, const Quad& q) {
    const int x0 = std::max(0, static_cast<int>(std::floor(q.x)));
    const int y0 = std::max(0, static_cast<int>(std::floor(q.y)));
    const int x1 = std::min(w, static_cast<int>(std::ceil(q.x + q.w)));
    const int y1 = std::min(h, static_cast<int>(std::ceil(q.y + q.h)));
    const Pixel src{q.r, q.g, q.b, q.a};
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            srcOver(buf[static_cast<std::size_t>(y * w + x)], src);
        }
    }
}

void rasterGroup(std::vector<Pixel>& dest, int w, int h, const Group& g);

void blitBuffer(std::vector<Pixel>& dest, int dw, int dh, const std::vector<Pixel>& src, int sw, int sh,
                Rect dstRect, float opacity) {
    const int x0 = std::max(0, static_cast<int>(std::floor(dstRect.origin.x)));
    const int y0 = std::max(0, static_cast<int>(std::floor(dstRect.origin.y)));
    const int x1 = std::min(dw, static_cast<int>(std::ceil(dstRect.origin.x + dstRect.size.x)));
    const int y1 = std::min(dh, static_cast<int>(std::ceil(dstRect.origin.y + dstRect.size.y)));
    for (int y = y0; y < y1; ++y) {
        const float v = sh <= 1 ? 0.f : (static_cast<float>(y) + 0.5f - dstRect.origin.y) / dstRect.size.y;
        const int sy = std::min(sh - 1, std::max(0, static_cast<int>(v * static_cast<float>(sh))));
        for (int x = x0; x < x1; ++x) {
            const float u = sw <= 1 ? 0.f : (static_cast<float>(x) + 0.5f - dstRect.origin.x) / dstRect.size.x;
            const int sx = std::min(sw - 1, std::max(0, static_cast<int>(u * static_cast<float>(sw))));
            Pixel p = src[static_cast<std::size_t>(sy * sw + sx)];
            p.r *= opacity;
            p.g *= opacity;
            p.b *= opacity;
            p.a *= opacity;
            srcOver(dest[static_cast<std::size_t>(y * dw + x)], p);
        }
    }
}

void paintMerged(std::vector<Pixel>& dest, int w, int h, const Group& g) {
    for (const Shape& s : g.shapes) {
        if (const auto* f = std::get_if<FillRect>(&s)) {
            fillRect(dest, w, h, *f);
        }
    }
    for (const auto& child : g.children) {
        if (child) {
            rasterGroup(dest, w, h, *child);
        }
    }
}

void rasterGroup(std::vector<Pixel>& dest, int w, int h, const Group& g) {
    if (needsIsolate(g)) {
        Rect b = g.params.bounds.size.x > 0 ? g.params.bounds : contentBounds(g);
        const int iw = std::max(1, static_cast<int>(std::ceil(b.size.x)));
        const int ih = std::max(1, static_cast<int>(std::ceil(b.size.y)));
        std::vector<Pixel> tmp(static_cast<std::size_t>(iw * ih), Pixel{0, 0, 0, 0});
        auto local = cloneGroup(g);
        local->params.opacity = 1.0f;
        local->params.isolate = false;
        local->params.transform = Mat4::identity();
        paintMerged(tmp, iw, ih, *local);
        const FillRect xf = transformFill(g.params.transform, FillRect{Rect{{0, 0}, b.size}, Color{}});
        blitBuffer(dest, w, h, tmp, iw, ih, xf.rect, g.params.opacity);
        return;
    }
    paintMerged(dest, w, h, g);
}

void rasterIsolate(std::vector<Pixel>& dest, int w, int h, const Isolate& iso);

void paintQuads(std::vector<Pixel>& dest, int w, int h, const std::vector<Quad>& quads,
                const std::vector<Isolate>& isolates) {
    for (const Quad& q : quads) {
        fillQuad(dest, w, h, q);
    }
    for (const Isolate& iso : isolates) {
        rasterIsolate(dest, w, h, iso);
    }
}

void rasterIsolate(std::vector<Pixel>& dest, int w, int h, const Isolate& iso) {
    std::vector<Pixel> tmp(static_cast<std::size_t>(iso.contentW * iso.contentH), Pixel{0, 0, 0, 0});
    paintQuads(tmp, iso.contentW, iso.contentH, iso.quads, iso.isolates);
    blitBuffer(dest, w, h, tmp, iso.contentW, iso.contentH,
               Rect{{iso.destX, iso.destY}, {iso.destW, iso.destH}}, iso.opacity);
}

void toRgba(const std::vector<Pixel>& buf, std::uint8_t* rgba) {
    for (std::size_t i = 0; i < buf.size(); ++i) {
        rgba[i * 4 + 0] = static_cast<std::uint8_t>(std::round(std::min(1.f, buf[i].r) * 255.f));
        rgba[i * 4 + 1] = static_cast<std::uint8_t>(std::round(std::min(1.f, buf[i].g) * 255.f));
        rgba[i * 4 + 2] = static_cast<std::uint8_t>(std::round(std::min(1.f, buf[i].b) * 255.f));
        rgba[i * 4 + 3] = static_cast<std::uint8_t>(std::round(std::min(1.f, buf[i].a) * 255.f));
    }
}

}  // namespace

void rasterScene(const Scene& scene, int width, int height, std::uint8_t* rgba) {
    thread_local std::vector<Pixel> buf;
    buf.assign(static_cast<std::size_t>(width * height), Pixel{0, 0, 0, 1});
    Group root = merge(scene.root, nullptr);
    rasterGroup(buf, width, height, root);
    toRgba(buf, rgba);
}

void rasterPacket(const FramePacket& packet, int width, int height, std::uint8_t* rgba) {
    thread_local std::vector<Pixel> buf;
    buf.assign(static_cast<std::size_t>(width * height), Pixel{0, 0, 0, 1});
    paintQuads(buf, width, height, packet.quads, packet.isolates);
    toRgba(buf, rgba);
}

}  // namespace glim::paint
