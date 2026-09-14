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
    const Vec4 premul = f.matter.color.premul();
    const Pixel src{premul.x, premul.y, premul.z, premul.w};
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

void rasterGroup(std::vector<Pixel>& dest, int w, int h, const Group& g, const ImageStore& images);

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

void blitImage(std::vector<Pixel>& dest, int w, int h, const Blit& b, const ImageStore& images,
               Vec4 tint) {
    const StoredImage* img = images.get(b.matter.imageId);
    if (!img) {
        return;
    }
    const int x0 = std::max(0, static_cast<int>(std::floor(b.rect.origin.x)));
    const int y0 = std::max(0, static_cast<int>(std::floor(b.rect.origin.y)));
    const int x1 = std::min(w, static_cast<int>(std::ceil(b.rect.origin.x + b.rect.size.x)));
    const int y1 = std::min(h, static_cast<int>(std::ceil(b.rect.origin.y + b.rect.size.y)));
    const float u0 = b.matter.uv.origin.x;
    const float v0 = b.matter.uv.origin.y;
    const float du = b.matter.uv.size.x;
    const float dv = b.matter.uv.size.y;
    const float iw = static_cast<float>(img->width);
    const float ih = static_cast<float>(img->height);
    for (int y = y0; y < y1; ++y) {
        const float fy = b.rect.size.y <= 0 ? 0.f
                                            : (static_cast<float>(y) + 0.5f - b.rect.origin.y) / b.rect.size.y;
        const float v = v0 + fy * dv;
        const int sy = std::min(img->height - 1, std::max(0, static_cast<int>(v * ih)));
        for (int x = x0; x < x1; ++x) {
            const float fx = b.rect.size.x <= 0 ? 0.f
                                                : (static_cast<float>(x) + 0.5f - b.rect.origin.x) / b.rect.size.x;
            const float u = u0 + fx * du;
            const int sx = std::min(img->width - 1, std::max(0, static_cast<int>(u * iw)));
            const std::uint8_t* px = img->rgba.data() + static_cast<std::size_t>((sy * img->width + sx) * 4);
            Pixel src{px[0] / 255.f, px[1] / 255.f, px[2] / 255.f, px[3] / 255.f};
            src.r *= src.a * tint.x;
            src.g *= src.a * tint.y;
            src.b *= src.a * tint.z;
            src.a *= tint.w;
            srcOver(dest[static_cast<std::size_t>(y * w + x)], src);
        }
    }
}

void paintMerged(std::vector<Pixel>& dest, int w, int h, const Group& g, const ImageStore& images) {
    for (const Shape& s : g.shapes) {
        if (const auto* f = std::get_if<FillRect>(&s)) {
            fillRect(dest, w, h, *f);
        } else if (const auto* blit = std::get_if<Blit>(&s)) {
            blitImage(dest, w, h, *blit, images, blit->matter.color.premul());
        }
    }
    for (const auto& child : g.children) {
        if (child) {
            rasterGroup(dest, w, h, *child, images);
        }
    }
}

void rasterGroup(std::vector<Pixel>& dest, int w, int h, const Group& g, const ImageStore& images) {
    if (needsIsolate(g)) {
        Rect b = g.params.bounds.size.x > 0 ? g.params.bounds : contentBounds(g);
        const int iw = std::max(1, static_cast<int>(std::ceil(b.size.x)));
        const int ih = std::max(1, static_cast<int>(std::ceil(b.size.y)));
        std::vector<Pixel> tmp(static_cast<std::size_t>(iw * ih), Pixel{0, 0, 0, 0});
        auto local = cloneGroup(g);
        local->params.opacity = 1.0f;
        local->params.isolate = false;
        local->params.transform = Mat4::identity();
        paintMerged(tmp, iw, ih, *local, images);
        const Rect xf = transformRect(g.params.transform, Rect{{0, 0}, b.size});
        blitBuffer(dest, w, h, tmp, iw, ih, xf, g.params.opacity);
        return;
    }
    paintMerged(dest, w, h, g, images);
}

void rasterIsolate(std::vector<Pixel>& dest, int w, int h, const Isolate& iso, const ImageStore& images);

void paintBlits(std::vector<Pixel>& dest, int w, int h, const std::vector<BlitQuad>& blits,
                const ImageStore& images) {
    for (const BlitQuad& q : blits) {
        Blit b;
        b.rect = {{q.x, q.y}, {q.w, q.h}};
        b.matter.kind = MatterKind::Sampled;
        b.matter.imageId = q.imageId;
        b.matter.uv = {{q.u0, q.v0}, {q.u1 - q.u0, q.v1 - q.v0}};
        blitImage(dest, w, h, b, images, {q.r, q.g, q.b, q.a});
    }
}

void paintQuads(std::vector<Pixel>& dest, int w, int h, const std::vector<Quad>& quads,
                const std::vector<BlitQuad>& blits, const std::vector<Isolate>& isolates,
                const ImageStore& images) {
    for (const Quad& q : quads) {
        fillQuad(dest, w, h, q);
    }
    paintBlits(dest, w, h, blits, images);
    for (const Isolate& iso : isolates) {
        rasterIsolate(dest, w, h, iso, images);
    }
}

void rasterIsolate(std::vector<Pixel>& dest, int w, int h, const Isolate& iso, const ImageStore& images) {
    std::vector<Pixel> tmp(static_cast<std::size_t>(iso.contentW * iso.contentH), Pixel{0, 0, 0, 0});
    paintQuads(tmp, iso.contentW, iso.contentH, iso.quads, iso.blits, iso.isolates, images);
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
    rasterGroup(buf, width, height, root, scene.images);
    toRgba(buf, rgba);
}

void rasterPacket(const FramePacket& packet, int width, int height, std::uint8_t* rgba) {
    thread_local std::vector<Pixel> buf;
    buf.assign(static_cast<std::size_t>(width * height), Pixel{0, 0, 0, 1});
    paintQuads(buf, width, height, packet.quads, packet.blits, packet.isolates, packet.images);
    toRgba(buf, rgba);
}

}  // namespace glim::paint
