#include <glim/paint/Software.h>

#include "Font.h"
#include "Strip.h"

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

void fillQuad(std::vector<Pixel>& buf, int w, int h, const Quad& q) {
    const float qx0 = q.x;
    const float qy0 = q.y;
    const float qx1 = q.x + q.w;
    const float qy1 = q.y + q.h;
    const int x0 = std::max(0, static_cast<int>(std::floor(qx0)));
    const int y0 = std::max(0, static_cast<int>(std::floor(qy0)));
    const int x1 = std::min(w, static_cast<int>(std::ceil(qx1)));
    const int y1 = std::min(h, static_cast<int>(std::ceil(qy1)));
    for (int y = y0; y < y1; ++y) {
        const float cy0 = std::max(static_cast<float>(y), qy0);
        const float cy1 = std::min(static_cast<float>(y + 1), qy1);
        const float ycov = cy1 - cy0;
        if (ycov <= 1e-6f) {
            continue;
        }
        for (int x = x0; x < x1; ++x) {
            const float cx0 = std::max(static_cast<float>(x), qx0);
            const float cx1 = std::min(static_cast<float>(x + 1), qx1);
            const float cov = (cx1 - cx0) * ycov;
            if (cov <= 1e-6f) {
                continue;
            }
            Pixel src{q.r * cov, q.g * cov, q.b * cov, q.a * cov};
            srcOver(buf[static_cast<std::size_t>(y * w + x)], src);
        }
    }
}

void fillQuads(std::vector<Pixel>& buf, int w, int h, const std::vector<Quad>& quads) {
    for (const Quad& q : quads) {
        fillQuad(buf, w, h, q);
    }
}

float clipCoverageAt(const ClipState& clip, float px, float py) {
    if (!clip.active) {
        return 1.f;
    }
    if (clip.rect.size.x <= 0.f || clip.rect.size.y <= 0.f) {
        return 0.f;
    }
    return roundedCoverage(clip.rect, clip.radius, px, py);
}

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

float sampleChannel(const StoredImage& img, float u, float v, int channel) {
    const float x = std::clamp(u, 0.f, 1.f) * static_cast<float>(std::max(1, img.width) - 1);
    const float y = std::clamp(v, 0.f, 1.f) * static_cast<float>(std::max(1, img.height) - 1);
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(img.width - 1, x0 + 1);
    const int y1 = std::min(img.height - 1, y0 + 1);
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    auto at = [&](int px, int py) {
        return img.rgba[static_cast<std::size_t>((py * img.width + px) * 4 + channel)] / 255.f;
    };
    const float a = at(x0, y0);
    const float b = at(x1, y0);
    const float c = at(x0, y1);
    const float d = at(x1, y1);
    return (a * (1.f - fx) + b * fx) * (1.f - fy) + (c * (1.f - fx) + d * fx) * fy;
}

void blitImage(std::vector<Pixel>& dest, int w, int h, const Blit& b, const ImageStore& images,
               Vec4 tint, const ClipState& clip, bool sdf) {
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
    const float texelsPerPx =
        b.rect.size.x > 1e-6f ? (std::fabs(du) * iw) / b.rect.size.x : 1.f;
    const float aa = std::max(0.03f, 0.6f * texelsPerPx * (static_cast<float>(kSdfOnEdge) / 255.f) /
                                          static_cast<float>(kSdfPad));
    for (int y = y0; y < y1; ++y) {
        const float fy = b.rect.size.y <= 0 ? 0.f
                                            : (static_cast<float>(y) + 0.5f - b.rect.origin.y) / b.rect.size.y;
        const float v = v0 + fy * dv;
        for (int x = x0; x < x1; ++x) {
            const float fx = b.rect.size.x <= 0 ? 0.f
                                                : (static_cast<float>(x) + 0.5f - b.rect.origin.x) / b.rect.size.x;
            const float u = u0 + fx * du;
            Pixel src;
            if (sdf) {
                const float d = sampleChannel(*img, u, v, 0);
                const float t = (d - (0.5f - aa)) / std::max(1e-6f, 2.f * aa);
                const float a = std::clamp(t, 0.f, 1.f);
                src = {tint.x * a, tint.y * a, tint.z * a, tint.w * a};
            } else {
                const int sy = std::min(img->height - 1, std::max(0, static_cast<int>(v * ih)));
                const int sx = std::min(img->width - 1, std::max(0, static_cast<int>(u * iw)));
                const std::uint8_t* px =
                    img->rgba.data() + static_cast<std::size_t>((sy * img->width + sx) * 4);
                src = {px[0] / 255.f, px[1] / 255.f, px[2] / 255.f, px[3] / 255.f};
                src.r *= src.a * tint.x;
                src.g *= src.a * tint.y;
                src.b *= src.a * tint.z;
                src.a *= tint.w;
            }
            const float cov = clipCoverageAt(clip, static_cast<float>(x) + 0.5f,
                                             static_cast<float>(y) + 0.5f);
            if (cov <= 0.f) {
                continue;
            }
            src.r *= cov;
            src.g *= cov;
            src.b *= cov;
            src.a *= cov;
            srcOver(dest[static_cast<std::size_t>(y * w + x)], src);
        }
    }
}

void rasterGroup(std::vector<Pixel>& dest, int w, int h, const Group& g, const ImageStore& images,
                 const Mat4& extra, const ClipState& parentClip);

void paintShapes(std::vector<Pixel>& dest, int w, int h, const Group& g, const ImageStore& images,
                 const Mat4& world, const ClipState& clip) {
    for (const Shape& s : g.shapes) {
        const Shape xf = transformShape(world, s);
        if (const auto* blit = std::get_if<Blit>(&xf)) {
            blitImage(dest, w, h, *blit, images, blit->matter.color.premul(), clip, false);
            continue;
        }
        if (std::get_if<SlotHole>(&xf)) {
            continue;
        }
        if (std::get_if<GlyphRun>(&xf)) {
            std::vector<Quad> unused;
            std::vector<BlitQuad> blits;
            appendShape(unused, blits, xf, clip);
            for (const BlitQuad& q : blits) {
                Blit b;
                b.rect = {{q.x, q.y}, {q.w, q.h}};
                b.matter.kind = MatterKind::Sampled;
                b.matter.imageId = q.imageId;
                b.matter.uv = {{q.u0, q.v0}, {q.u1 - q.u0, q.v1 - q.v0}};
                blitImage(dest, w, h, b, images, {q.r, q.g, q.b, q.a}, clip, q.sdf != 0);
            }
            continue;
        }
        std::vector<Quad> quads;
        std::vector<BlitQuad> unused;
        appendShape(quads, unused, xf, clip);
        fillQuads(dest, w, h, quads);
    }
    for (const auto& child : g.children) {
        if (child) {
            rasterGroup(dest, w, h, *child, images, world, clip);
        }
    }
}

void rasterGroup(std::vector<Pixel>& dest, int w, int h, const Group& g, const ImageStore& images,
                 const Mat4& extra, const ClipState& parentClip) {
    const Mat4 world = extra * g.params.transform;
    const ClipState clip = intersectClip(parentClip, clipOf(g.params, world));
    if (needsIsolate(g)) {
        Rect b = g.params.bounds.size.x > 0 ? g.params.bounds : contentBounds(g);
        const int iw = std::max(1, static_cast<int>(std::ceil(b.size.x)));
        const int ih = std::max(1, static_cast<int>(std::ceil(b.size.y)));
        std::vector<Pixel> tmp(static_cast<std::size_t>(iw * ih), Pixel{0, 0, 0, 0});
        auto local = cloneGroup(g);
        local->params.opacity = 1.0f;
        local->params.isolate = false;
        local->params.transform = Mat4::identity();
        if (!hasClip(local->params) && b.size.x > 0.f && b.size.y > 0.f) {
            local->params.clip = Rect{{0, 0}, b.size};
        }
        paintShapes(tmp, iw, ih, *local, images, Mat4::identity(), clipOf(local->params, Mat4::identity()));
        const Rect xf = transformRect(world, Rect{{0, 0}, b.size});
        blitBuffer(dest, w, h, tmp, iw, ih, xf, g.params.opacity);
        return;
    }
    paintShapes(dest, w, h, g, images, world, clip);
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
        blitImage(dest, w, h, b, images, {q.r, q.g, q.b, q.a}, {}, q.sdf != 0);
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
    rasterGroup(buf, width, height, root, scene.images, Mat4::identity(), {});
    toRgba(buf, rgba);
}

void rasterPacket(const FramePacket& packet, int width, int height, std::uint8_t* rgba) {
    thread_local std::vector<Pixel> buf;
    buf.assign(static_cast<std::size_t>(width * height), Pixel{0, 0, 0, 1});
    paintQuads(buf, width, height, packet.quads, packet.blits, packet.isolates, packet.images);
    toRgba(buf, rgba);
}

}  // namespace glim::paint
