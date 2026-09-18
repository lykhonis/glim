#include <glim/paint/Software.h>

#include "Font.h"
#include "Strip.h"

#include <glim/assert.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace glim::paint {

thread_local RasterOptions tlsRaster{};

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
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    const bool opaque = q.a >= 0.999f;
    const bool aligned = qx0 == static_cast<float>(x0) && qy0 == static_cast<float>(y0) &&
                         qx1 == static_cast<float>(x1) && qy1 == static_cast<float>(y1);
    if (opaque && aligned) {
        const Pixel src{q.r, q.g, q.b, 1.f};
        for (int y = y0; y < y1; ++y) {
            Pixel* row = buf.data() + static_cast<std::size_t>(y * w + x0);
            for (int x = 0; x < x1 - x0; ++x) {
                row[x] = src;
            }
        }
        return;
    }
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

Pixel samplePixel(const std::vector<Pixel>& img, int w, int h, float px, float py);

void blitBuffer(std::vector<Pixel>& dest, int dw, int dh, const std::vector<Pixel>& src, int sw, int sh,
                Rect dstRect, float opacity) {
    const int x0 = std::max(0, static_cast<int>(std::floor(dstRect.origin.x)));
    const int y0 = std::max(0, static_cast<int>(std::floor(dstRect.origin.y)));
    const int x1 = std::min(dw, static_cast<int>(std::ceil(dstRect.origin.x + dstRect.size.x)));
    const int y1 = std::min(dh, static_cast<int>(std::ceil(dstRect.origin.y + dstRect.size.y)));
    const bool aligned = dstRect.origin.x == static_cast<float>(static_cast<int>(dstRect.origin.x)) &&
                         dstRect.origin.y == static_cast<float>(static_cast<int>(dstRect.origin.y)) &&
                         dstRect.size.x == static_cast<float>(sw) &&
                         dstRect.size.y == static_cast<float>(sh);
    const int ox = static_cast<int>(dstRect.origin.x);
    const int oy = static_cast<int>(dstRect.origin.y);
    for (int y = y0; y < y1; ++y) {
        if (aligned) {
            const int sy = std::min(sh - 1, std::max(0, y - oy));
            const Pixel* row = src.data() + static_cast<std::size_t>(sy * sw);
            for (int x = x0; x < x1; ++x) {
                const int sx = std::min(sw - 1, std::max(0, x - ox));
                Pixel p = row[sx];
                p.r *= opacity;
                p.g *= opacity;
                p.b *= opacity;
                p.a *= opacity;
                srcOver(dest[static_cast<std::size_t>(y * dw + x)], p);
            }
            continue;
        }
        const float py = sh <= 1 ? 0.5f
                                 : (static_cast<float>(y) + 0.5f - dstRect.origin.y) / dstRect.size.y *
                                       static_cast<float>(sh);
        for (int x = x0; x < x1; ++x) {
            const float px = sw <= 1 ? 0.5f
                                     : (static_cast<float>(x) + 0.5f - dstRect.origin.x) / dstRect.size.x *
                                           static_cast<float>(sw);
            Pixel p = samplePixel(src, sw, sh, px, py);
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
    if (!img || img->rgba.empty()) {
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
                 const Mat4& extra, const ClipState& parentClip, float pixelRatio);

float sdSquircle(float px, float py, float hx, float hy, float r) {
    r = std::min(r, std::min(hx, hy));
    const float qx = std::fabs(px) - hx + r;
    const float qy = std::fabs(py) - hy + r;
    const float mx = std::max(qx, 0.f);
    const float my = std::max(qy, 0.f);
    return std::pow(std::pow(mx, 4.f) + std::pow(my, 4.f), 0.25f) + std::min(std::max(qx, qy), 0.f) - r;
}

float smin(float a, float b, float k) {
    if (k <= 0.001f) {
        return std::min(a, b);
    }
    const float h = std::max(k - std::fabs(a - b), 0.f) / k;
    return std::min(a, b) - h * h * k * 0.25f;
}

struct PlateSpec {
    float sigma = 0;
    float bend = 0;
    float mergeK = 0;
    float press = 0;
    float lightX = 0.35f;
    float lightY = 0.8f;
    float lightZ = 0.5f;
    bool flat = false;
    int n = 0;
    BackdropPill pills[kMaxBackdropPills]{};
};

float fieldSdf(const PlateSpec& spec, float px, float py, float plateW, float plateH) {
    float sdf = 1e6f;
    const int n = spec.n > 0 ? spec.n : 1;
    for (int i = 0; i < n; ++i) {
        const BackdropPill& pill = spec.pills[i];
        const float cx = pill.rect.origin.x + pill.rect.size.x * 0.5f;
        const float cy = pill.rect.origin.y + pill.rect.size.y * 0.5f;
        const float d = sdSquircle(px - cx, py - cy, pill.rect.size.x * 0.5f, pill.rect.size.y * 0.5f,
                                   pill.radius);
        sdf = (i == 0) ? d : smin(sdf, d, spec.mergeK);
    }
    sdf += spec.press * 0.035f * std::min(plateW, plateH);
    return sdf;
}

Pixel samplePixel(const std::vector<Pixel>& img, int w, int h, float px, float py) {
    if (w <= 0 || h <= 0 || img.empty()) {
        return Pixel{0, 0, 0, 0};
    }
    const float x = std::clamp(px - 0.5f, 0.f, static_cast<float>(w - 1));
    const float y = std::clamp(py - 0.5f, 0.f, static_cast<float>(h - 1));
    const int x0 = static_cast<int>(x);
    const int y0 = static_cast<int>(y);
    const int x1 = std::min(w - 1, x0 + 1);
    const int y1 = std::min(h - 1, y0 + 1);
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const Pixel a = img[static_cast<std::size_t>(y0 * w + x0)];
    const Pixel b = img[static_cast<std::size_t>(y0 * w + x1)];
    const Pixel c = img[static_cast<std::size_t>(y1 * w + x0)];
    const Pixel d = img[static_cast<std::size_t>(y1 * w + x1)];
    const float ia = 1.f - fx;
    const float ib = 1.f - fy;
    return Pixel{(a.r * ia + b.r * fx) * ib + (c.r * ia + d.r * fx) * fy,
                 (a.g * ia + b.g * fx) * ib + (c.g * ia + d.g * fx) * fy,
                 (a.b * ia + b.b * fx) * ib + (c.b * ia + d.b * fx) * fy,
                 (a.a * ia + b.a * fx) * ib + (c.a * ia + d.a * fx) * fy};
}

Pixel sampleDest(const std::vector<Pixel>& dest, int dw, int dh, float sx, float sy) {
    const int ix = std::min(dw - 1, std::max(0, static_cast<int>(sx)));
    const int iy = std::min(dh - 1, std::max(0, static_cast<int>(sy)));
    return dest[static_cast<std::size_t>(iy * dw + ix)];
}

Pixel sampleDestBilinear(const std::vector<Pixel>& dest, int dw, int dh, float sx, float sy) {
    return samplePixel(dest, dw, dh, sx, sy);
}

Pixel sampleDestBlur(const std::vector<Pixel>& dest, int dw, int dh, float sx, float sy, float sigma) {
    if (sigma <= 0.5f) {
        return sampleDest(dest, dw, dh, sx, sy);
    }
    Pixel acc{0, 0, 0, 0};
    float wt = 0.f;
    auto add = [&](float x, float y, float w) {
        const Pixel s = sampleDest(dest, dw, dh, x, y);
        acc.r += s.r * w;
        acc.g += s.g * w;
        acc.b += s.b * w;
        acc.a += s.a * w;
        wt += w;
    };
    add(sx, sy, 0.227027f);
    add(sx + sigma, sy, 0.1945946f);
    add(sx - sigma, sy, 0.1945946f);
    add(sx, sy + sigma, 0.1945946f);
    add(sx, sy - sigma, 0.1945946f);
    add(sx + sigma, sy + sigma, 0.1216216f);
    add(sx - sigma, sy - sigma, 0.1216216f);
    add(sx + sigma, sy - sigma, 0.1216216f);
    add(sx - sigma, sy + sigma, 0.1216216f);
    if (wt > 0.f) {
        acc.r /= wt;
        acc.g /= wt;
        acc.b /= wt;
        acc.a /= wt;
    }
    return acc;
}

PlateSpec specFromGroup(const Group& g, Rect surface, float pr) {
    PlateSpec spec;
    spec.sigma = snapBackdropSigma(g.params.backdropBlur) * pr * 0.5f;
    spec.bend = g.params.backdropBend;
    spec.mergeK = g.params.backdropMerge * pr;
    spec.press = g.params.backdropPress;
    spec.lightX = g.params.backdropLightX;
    spec.lightY = g.params.backdropLightY;
    spec.lightZ = g.params.backdropLightZ;
    spec.flat = g.params.backdropFlat;
    BackdropPill pills[kMaxBackdropPills];
    spec.n = collectBackdropPills(g, pills);
    for (int i = 0; i < spec.n; ++i) {
        spec.pills[i] = pills[i];
        spec.pills[i].rect.origin.x = (pills[i].rect.origin.x - surface.origin.x) * pr;
        spec.pills[i].rect.origin.y = (pills[i].rect.origin.y - surface.origin.y) * pr;
        spec.pills[i].rect.size.x *= pr;
        spec.pills[i].rect.size.y *= pr;
        spec.pills[i].radius *= pr;
    }
    return spec;
}

PlateSpec specFromIsolate(const Isolate& iso, float pr) {
    PlateSpec spec;
    spec.sigma = iso.backdropSigma * pr * 0.5f;
    spec.bend = iso.backdropBend;
    spec.mergeK = iso.backdropMerge * pr;
    spec.press = iso.backdropPress;
    spec.lightX = iso.backdropLightX;
    spec.lightY = iso.backdropLightY;
    spec.lightZ = iso.backdropLightZ;
    spec.flat = iso.backdropFlat;
    spec.n = std::min(kMaxBackdropPills, static_cast<int>(iso.backdropPills.size()));
    const float sx = iso.destW > 0.f ? static_cast<float>(iso.contentW) / iso.destW : pr;
    const float sy = iso.destH > 0.f ? static_cast<float>(iso.contentH) / iso.destH : pr;
    for (int i = 0; i < spec.n; ++i) {
        spec.pills[i] = iso.backdropPills[static_cast<std::size_t>(i)];
        spec.pills[i].rect.origin.x *= sx;
        spec.pills[i].rect.origin.y *= sy;
        spec.pills[i].rect.size.x *= sx;
        spec.pills[i].rect.size.y *= sy;
        spec.pills[i].radius *= std::min(sx, sy);
    }
    return spec;
}

void samplePlate(std::vector<Pixel>& plate, int iw, int ih, const std::vector<Pixel>& dest, int dw, int dh,
                 Rect src, const PlateSpec& spec) {
    if (iw <= 0 || ih <= 0 || dw <= 0 || dh <= 0 || src.size.x <= 0.f || src.size.y <= 0.f) {
        return;
    }
    const float plateW = static_cast<float>(iw);
    const float plateH = static_cast<float>(ih);
    const float minSide = std::max(1.f, std::min(plateW, plateH));
    const float thickness = std::max(18.f, minSide * 0.42f);
    PlateSpec field = spec;
    if (field.n <= 0) {
        field.n = 1;
        field.pills[0].rect = Rect::fromSize({plateW, plateH});
    }
    for (int y = 0; y < ih; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / plateH;
        const float py = static_cast<float>(y) + 0.5f;
        for (int x = 0; x < iw; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / plateW;
            const float px = static_cast<float>(x) + 0.5f;
            const float sdf = fieldSdf(field, px, py, plateW, plateH);
            const float mask = std::clamp(0.5f - sdf, 0.f, 1.f);
            if (mask < 0.001f) {
                plate[static_cast<std::size_t>(y * iw + x)] = Pixel{0, 0, 0, 0};
                continue;
            }
            float sx = src.origin.x + u * src.size.x;
            float sy = src.origin.y + v * src.size.y;
            float edge = 0.f;
            float nx = 0.f;
            float ny = 0.f;
            float nz = 1.f;
            if (spec.bend > 0.f && !spec.flat) {
                const float height = std::clamp(-sdf / thickness, 0.f, 1.f);
                const float gx = fieldSdf(field, px + 1.f, py, plateW, plateH) -
                                 fieldSdf(field, px - 1.f, py, plateW, plateH);
                const float gy = fieldSdf(field, px, py + 1.f, plateW, plateH) -
                                 fieldSdf(field, px, py - 1.f, plateW, plateH);
                const float glen = std::max(1e-4f, std::hypot(gx, gy));
                nx = gx / glen;
                ny = gy / glen;
                edge = 1.f - height;
                const float nxy = edge * 1.05f;
                nx *= nxy;
                ny *= nxy;
                const float nlen = std::max(1e-4f, std::sqrt(nx * nx + ny * ny + 1.f));
                nx /= nlen;
                ny /= nlen;
                nz = 1.f / nlen;
                const float mag = std::pow(edge, 4.f) * spec.bend * std::min(48.f, minSide * 0.30f);
                sx += nx * mag * (src.size.x / plateW);
                sy += ny * mag * (src.size.y / plateH);
            }
            Pixel p = sampleDestBlur(dest, dw, dh, sx, sy, spec.sigma);
            const float lum = p.r * 0.2126f + p.g * 0.7152f + p.b * 0.0722f;
            if (spec.flat) {
                const float tint = lum >= 0.45f ? 0.92f : 0.16f;
                p.r = tint;
                p.g = tint;
                p.b = tint;
                p.a = 1.f;
            } else {
                const float frost = 0.06f + 0.30f * std::clamp(spec.sigma / 12.f, 0.f, 1.f);
                p.r = p.r + (1.f - p.r) * frost;
                p.g = p.g + (1.f - p.g) * frost;
                p.b = p.b + (1.f - p.b) * frost;
                if (spec.bend > 0.f) {
                    const float Llen = std::sqrt(spec.lightX * spec.lightX + spec.lightY * spec.lightY +
                                                 spec.lightZ * spec.lightZ);
                    const float ndl = std::clamp(
                        (nx * spec.lightX + ny * spec.lightY + nz * spec.lightZ) / std::max(Llen, 1e-4f), 0.f,
                        1.f);
                    const float hi = std::pow(ndl, 72.f) * std::pow(edge, 2.4f) * 0.85f;
                    p.r += hi;
                    p.g += hi;
                    p.b += hi;
                }
            }
            p.r *= mask;
            p.g *= mask;
            p.b *= mask;
            p.a *= mask;
            plate[static_cast<std::size_t>(y * iw + x)] = p;
        }
    }
}

void blurSeparable(std::vector<Pixel>& img, int w, int h, float sigma) {
    const int radius = std::max(1, std::min(32, static_cast<int>(std::ceil(3.f * sigma))));
    thread_local std::vector<Pixel> tmp;
    tmp = img;
    auto blurAxis = [&](bool horiz) {
        const int n = horiz ? w : h;
        const int m = horiz ? h : w;
        for (int j = 0; j < m; ++j) {
            for (int i = 0; i < n; ++i) {
                Pixel acc{0, 0, 0, 0};
                float wt = 0.f;
                for (int k = -radius; k <= radius; ++k) {
                    const int p = std::min(n - 1, std::max(0, i + k));
                    const float d = static_cast<float>(k) / std::max(1.f, sigma);
                    const float ww = std::exp(-0.5f * d * d);
                    const Pixel s = horiz ? tmp[static_cast<std::size_t>(j * w + p)]
                                          : tmp[static_cast<std::size_t>(p * w + j)];
                    acc.r += s.r * ww;
                    acc.g += s.g * ww;
                    acc.b += s.b * ww;
                    acc.a += s.a * ww;
                    wt += ww;
                }
                if (wt > 0.f) {
                    acc.r /= wt;
                    acc.g /= wt;
                    acc.b /= wt;
                    acc.a /= wt;
                }
                Pixel& out = horiz ? img[static_cast<std::size_t>(j * w + i)]
                                   : img[static_cast<std::size_t>(i * w + j)];
                out = acc;
            }
        }
        tmp = img;
    };
    blurAxis(true);
    blurAxis(false);
}

void blurCheap(std::vector<Pixel>& img, int w, int h, float sigma) {
    if (sigma < 0.45f || w < 2 || h < 2 || img.empty()) {
        return;
    }
    const int scale = sigma >= 18.f ? 4 : (sigma >= 3.f ? 2 : 1);
    if (scale == 1) {
        blurSeparable(img, w, h, sigma);
        return;
    }
    const int dw = std::max(1, w / scale);
    const int dh = std::max(1, h / scale);
    thread_local std::vector<Pixel> small;
    small.assign(static_cast<std::size_t>(dw * dh), Pixel{0, 0, 0, 0});
    for (int y = 0; y < dh; ++y) {
        const int y0 = y * scale;
        const int y1 = std::min(h, y0 + scale);
        for (int x = 0; x < dw; ++x) {
            const int x0 = x * scale;
            const int x1 = std::min(w, x0 + scale);
            Pixel acc{0, 0, 0, 0};
            int n = 0;
            for (int yy = y0; yy < y1; ++yy) {
                for (int xx = x0; xx < x1; ++xx) {
                    const Pixel s = img[static_cast<std::size_t>(yy * w + xx)];
                    acc.r += s.r;
                    acc.g += s.g;
                    acc.b += s.b;
                    acc.a += s.a;
                    ++n;
                }
            }
            if (n > 0) {
                const float inv = 1.f / static_cast<float>(n);
                acc.r *= inv;
                acc.g *= inv;
                acc.b *= inv;
                acc.a *= inv;
            }
            small[static_cast<std::size_t>(y * dw + x)] = acc;
        }
    }
    blurSeparable(small, dw, dh, sigma / static_cast<float>(scale));
    for (int y = 0; y < h; ++y) {
        const float py = (static_cast<float>(y) + 0.5f) * static_cast<float>(dh) / static_cast<float>(h);
        for (int x = 0; x < w; ++x) {
            const float px = (static_cast<float>(x) + 0.5f) * static_cast<float>(dw) / static_cast<float>(w);
            img[static_cast<std::size_t>(y * w + x)] = samplePixel(small, dw, dh, px, py);
        }
    }
}

void copyDestPlate(std::vector<Pixel>& plate, int iw, int ih, const std::vector<Pixel>& dest, int dw, int dh,
                   Rect src) {
    const float plateW = static_cast<float>(iw);
    const float plateH = static_cast<float>(ih);
    const bool aligned = src.origin.x == static_cast<float>(static_cast<int>(src.origin.x)) &&
                         src.origin.y == static_cast<float>(static_cast<int>(src.origin.y)) &&
                         src.size.x == plateW && src.size.y == plateH;
    if (aligned) {
        const int ox = static_cast<int>(src.origin.x);
        const int oy = static_cast<int>(src.origin.y);
        for (int y = 0; y < ih; ++y) {
            const int dy = std::min(dh - 1, std::max(0, oy + y));
            const Pixel* row = dest.data() + static_cast<std::size_t>(dy * dw);
            Pixel* out = plate.data() + static_cast<std::size_t>(y * iw);
            for (int x = 0; x < iw; ++x) {
                const int dx = std::min(dw - 1, std::max(0, ox + x));
                out[x] = row[dx];
            }
        }
        return;
    }
    for (int y = 0; y < ih; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / plateH;
        const float sy = src.origin.y + v * src.size.y;
        for (int x = 0; x < iw; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / plateW;
            const float sx = src.origin.x + u * src.size.x;
            plate[static_cast<std::size_t>(y * iw + x)] = sampleDestBilinear(dest, dw, dh, sx, sy);
        }
    }
}

struct GlassSdf {
    float d = 1e6f;
    float nx = 0.f;
    float ny = 1.f;
};

GlassSdf sdRoundBoxN(float px, float py, float hx, float hy, float r) {
    r = std::min(r, std::min(hx, hy));
    const float ax = std::fabs(px);
    const float ay = std::fabs(py);
    const float qx = ax - hx + r;
    const float qy = ay - hy + r;
    const float sx = px < 0.f ? -1.f : 1.f;
    const float sy = py < 0.f ? -1.f : 1.f;
    GlassSdf o;
    if (qx > 0.f && qy > 0.f) {
        const float len = std::sqrt(qx * qx + qy * qy);
        o.d = len - r;
        const float inv = 1.f / std::max(len, 1e-4f);
        o.nx = qx * inv * sx;
        o.ny = qy * inv * sy;
    } else if (qx > qy) {
        o.d = qx - r;
        o.nx = sx;
        o.ny = 0.f;
    } else {
        o.d = qy - r;
        o.nx = 0.f;
        o.ny = sy;
    }
    return o;
}

GlassSdf glassFieldEval(const PlateSpec& spec, float px, float py) {
    GlassSdf best;
    const int n = spec.n > 0 ? spec.n : 1;
    for (int i = 0; i < n; ++i) {
        const BackdropPill& pill = spec.pills[i];
        const float hx = pill.rect.size.x * 0.5f;
        const float hy = pill.rect.size.y * 0.5f;
        const float cx = pill.rect.origin.x + hx;
        const float cy = pill.rect.origin.y + hy;
        const float bx = std::fabs(px - cx) - hx;
        const float by = std::fabs(py - cy) - hy;
        if (bx > 1.4f || by > 1.4f) {
            const float outside = bx > by ? bx : by;
            if (i == 0 || outside < best.d) {
                best.d = outside;
                best.nx = 0.f;
                best.ny = 1.f;
            }
            continue;
        }
        GlassSdf s = sdRoundBoxN(px - cx, py - cy, hx, hy, pill.radius);
        if (spec.mergeK > 0.001f && i > 0) {
            s.d = smin(best.d, s.d, spec.mergeK);
        }
        if (i == 0 || s.d < best.d) {
            best = s;
        }
    }
    return best;
}

Pixel shadeGlass(Pixel p, const GlassSdf& f, const Glass& mat, float mask) {
    const bool flatten = mat.flatten || mat.variant == GlassVariant::Identity;
    if (flatten) {
        p.r = 0.94f;
        p.g = 0.94f;
        p.b = 0.94f;
        p.a = 1.f;
        const float rim = std::clamp(1.6f - std::fabs(f.d), 0.f, 1.f) * 0.05f;
        p.r = p.r + (1.f - p.r) * rim;
        p.g = p.g + (1.f - p.g) * rim;
        p.b = p.b + (1.f - p.b) * rim;
    } else {
        const bool regular = mat.variant == GlassVariant::Regular;
        const float milk = regular ? 0.22f : 0.08f;
        p.r = p.r + (1.f - p.r) * milk;
        p.g = p.g + (1.f - p.g) * milk;
        p.b = p.b + (1.f - p.b) * milk;
        const float ta = mat.tint.alpha() * 0.12f;
        p.r = p.r + (mat.tint.red() - p.r) * ta;
        p.g = p.g + (mat.tint.green() - p.g) * ta;
        p.b = p.b + (mat.tint.blue() - p.b) * ta;
        if (mask > 0.85f && f.d > -5.f) {
            const float inside = std::max(-f.d, 0.f);
            const float hair = std::clamp(1.f - std::fabs(inside - 1.2f) / 1.8f, 0.f, 1.f);
            const float hair2 = hair * hair * hair * hair * hair;
            const float horiz = f.nx * f.nx;
            p.r *= 1.f - horiz * hair2 * 0.38f;
            p.g *= 1.f - horiz * hair2 * 0.38f;
            p.b *= 1.f - horiz * hair2 * 0.38f;
            const float top = std::clamp(-f.ny, 0.f, 1.f);
            const float bot = std::clamp(f.ny, 0.f, 1.f);
            const float hi = hair2 * (top + bot * 0.62f);
            p.r = p.r + (1.f - p.r) * hi;
            p.g = p.g + (1.f - p.g) * hi;
            p.b = p.b + (1.f - p.b) * hi;
            const float rim = std::clamp(1.f - std::fabs(f.d + 0.4f) / 2.4f, 0.f, 1.f);
            const float rimA = rim * rim * 0.12f;
            p.r = p.r + (1.f - p.r) * rimA;
            p.g = p.g + (1.f - p.g) * rimA;
            p.b = p.b + (1.f - p.b) * rimA;
        }
    }
    p.r *= mask;
    p.g *= mask;
    p.b *= mask;
    p.a *= mask;
    return p;
}

void maskFrostPlate(std::vector<Pixel>& plate, int iw, int ih, const PlateSpec& spec) {
    const float frost = spec.flat ? 0.f : 0.07f + 0.12f * std::clamp(spec.sigma / 16.f, 0.f, 1.f);
    PlateSpec field = spec;
    if (field.n <= 0) {
        field.n = 1;
        field.pills[0].rect = Rect::fromSize({static_cast<float>(iw), static_cast<float>(ih)});
    }
    for (int y = 0; y < ih; ++y) {
        const float py = static_cast<float>(y) + 0.5f;
        for (int x = 0; x < iw; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const GlassSdf f = glassFieldEval(field, px, py);
            const float mask = std::clamp((1.5f - f.d) / 1.5f, 0.f, 1.f);
            const std::size_t i = static_cast<std::size_t>(y * iw + x);
            if (mask < 0.001f) {
                plate[i] = Pixel{0, 0, 0, 0};
                continue;
            }
            Pixel p = plate[i];
            if (spec.flat) {
                const float lum = p.r * 0.2126f + p.g * 0.7152f + p.b * 0.0722f;
                const float tint = lum >= 0.45f ? 0.92f : 0.16f;
                p.r = tint;
                p.g = tint;
                p.b = tint;
                p.a = 1.f;
            } else if (frost > 0.f) {
                p.r = p.r + (1.f - p.r) * frost;
                p.g = p.g + (1.f - p.g) * frost;
                p.b = p.b + (1.f - p.b) * frost;
            }
            p.r *= mask;
            p.g *= mask;
            p.b *= mask;
            p.a *= mask;
            plate[i] = p;
        }
    }
}

void applyGlassChrome(std::vector<Pixel>& plate, int iw, int ih, const PlateSpec& field, const Glass& mat) {
    for (int y = 0; y < ih; ++y) {
        const float py = static_cast<float>(y) + 0.5f;
        for (int x = 0; x < iw; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const GlassSdf f = glassFieldEval(field, px, py);
            const float mask = std::clamp((1.5f - f.d) / 1.5f, 0.f, 1.f);
            const std::size_t i = static_cast<std::size_t>(y * iw + x);
            if (mask < 0.001f) {
                plate[i] = Pixel{0, 0, 0, 0};
                continue;
            }
            plate[i] = shadeGlass(plate[i], f, mat, mask);
        }
    }
}

PlateSpec glassPillsToField(const GlassPill* pills, int n, float mergeKPx, float pr, const Rect& surface) {
    PlateSpec spec;
    spec.mergeK = tlsRaster.glassMerge ? mergeKPx * pr : 0.f;
    if (n <= 0) {
        spec.n = 1;
        spec.pills[0].rect = Rect::fromSize({surface.size.x * pr, surface.size.y * pr});
        spec.pills[0].radius = std::min(spec.pills[0].rect.size.x, spec.pills[0].rect.size.y) * 0.5f;
        return spec;
    }
    spec.n = std::min(kMaxBackdropPills, n);
    for (int i = 0; i < spec.n; ++i) {
        spec.pills[i].rect.origin.x = (pills[i].rect.origin.x - surface.origin.x) * pr;
        spec.pills[i].rect.origin.y = (pills[i].rect.origin.y - surface.origin.y) * pr;
        spec.pills[i].rect.size.x = pills[i].rect.size.x * pr;
        spec.pills[i].rect.size.y = pills[i].rect.size.y * pr;
        spec.pills[i].radius = pills[i].radius * pr;
    }
    return spec;
}

Pixel sampleCheapFrost(const std::vector<Pixel>& dest, int dw, int dh, float sx, float sy, float radius) {
    if (radius < 0.45f) {
        return sampleDestBilinear(dest, dw, dh, sx, sy);
    }
    Pixel acc{0, 0, 0, 0};
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const Pixel s = sampleDest(dest, dw, dh, sx + static_cast<float>(dx) * radius,
                                       sy + static_cast<float>(dy) * radius);
            acc.r += s.r;
            acc.g += s.g;
            acc.b += s.b;
            acc.a += s.a;
        }
    }
    const float inv = 1.f / 9.f;
    return Pixel{acc.r * inv, acc.g * inv, acc.b * inv, acc.a * inv};
}

void sampleGlassPlate(std::vector<Pixel>& plate, int iw, int ih, const std::vector<Pixel>& dest, int dw,
                      int dh, Rect src, const Glass& mat, const PlateSpec& field) {
    if (iw <= 0 || ih <= 0 || dw <= 0 || dh <= 0 || src.size.x <= 0.f || src.size.y <= 0.f) {
        return;
    }
    const bool flatten = mat.flatten || mat.variant == GlassVariant::Identity;
    if (tlsRaster.glassBlur && !flatten) {
        copyDestPlate(plate, iw, ih, dest, dw, dh, src);
        blurCheap(plate, iw, ih, mat.variant == GlassVariant::Clear ? 2.5f : 6.f);
        applyGlassChrome(plate, iw, ih, field, mat);
        return;
    }
    const float plateW = static_cast<float>(iw);
    const float plateH = static_cast<float>(ih);
    const float frostR = flatten ? 0.f : (mat.variant == GlassVariant::Regular ? 3.25f : 0.f);
    const bool aligned = src.origin.x == static_cast<float>(static_cast<int>(src.origin.x)) &&
                         src.origin.y == static_cast<float>(static_cast<int>(src.origin.y)) &&
                         src.size.x == plateW && src.size.y == plateH;
    const int ox = static_cast<int>(src.origin.x);
    const int oy = static_cast<int>(src.origin.y);
    for (int y = 0; y < ih; ++y) {
        const float py = static_cast<float>(y) + 0.5f;
        const float sy = aligned ? 0.f : src.origin.y + py / plateH * src.size.y;
        for (int x = 0; x < iw; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const GlassSdf f = glassFieldEval(field, px, py);
            const float mask = std::clamp((1.5f - f.d) / 1.5f, 0.f, 1.f);
            const std::size_t i = static_cast<std::size_t>(y * iw + x);
            if (mask < 0.001f) {
                plate[i] = Pixel{0, 0, 0, 0};
                continue;
            }
            Pixel p;
            if (aligned) {
                const float sx = static_cast<float>(ox + x) + 0.5f;
                const float syc = static_cast<float>(oy + y) + 0.5f;
                p = frostR > 0.f ? sampleCheapFrost(dest, dw, dh, sx, syc, frostR)
                                 : sampleDest(dest, dw, dh, sx, syc);
            } else {
                const float sx = src.origin.x + px / plateW * src.size.x;
                p = frostR > 0.f ? sampleCheapFrost(dest, dw, dh, sx, sy, frostR)
                                 : sampleDestBilinear(dest, dw, dh, sx, sy);
            }
            plate[i] = shadeGlass(p, f, mat, mask);
        }
    }
}

void paintShapes(std::vector<Pixel>& dest, int w, int h, const Group& g, const ImageStore& images,
                 const Mat4& world, const ClipState& clip, float pixelRatio) {
    bool seenBackdrop = false;
    bool afterBackdrop = false;
    bool seenGlass = false;
    bool afterGlass = false;
    visitGroup(
        g,
        [&](const Shape& s) {
            const Shape xf = transformShape(world, s);
            if (const auto* blit = std::get_if<Blit>(&xf)) {
                blitImage(dest, w, h, *blit, images, blit->matter.color.premul(), clip, false);
                return;
            }
            if (std::get_if<SlotHole>(&xf)) {
                return;
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
                return;
            }
            std::vector<Quad> quads;
            std::vector<BlitQuad> unused;
            appendShape(quads, unused, xf, clip);
            fillQuads(dest, w, h, quads);
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
            rasterGroup(dest, w, h, child, images, world, clip, pixelRatio);
        });
}

void rasterGroup(std::vector<Pixel>& dest, int w, int h, const Group& g, const ImageStore& images,
                 const Mat4& extra, const ClipState& parentClip, float pixelRatio) {
    const Mat4 world = extra * g.params.transform;
    const ClipState clip = intersectClip(parentClip, clipOf(g.params, world));
    if (needsIsolate(g)) {
        const bool backdrop = hasBackdrop(g);
        const bool glassWork = hasGlassWork(g);
        const Rect surface = glassWork ? glassSurface(g)
                                       : (g.params.bounds.size.x > 0.f ? g.params.bounds : contentBounds(g));
        const float pr = pixelRatio > 0.f ? pixelRatio : 1.f;
        const int iw = isolatePixelSize(surface.size.x, pr);
        const int ih = isolatePixelSize(surface.size.y, pr);
        std::vector<Pixel> tmp(static_cast<std::size_t>(iw * ih), Pixel{0, 0, 0, 0});
        const Rect xf = transformRect(world, surface);
        if (glassWork) {
            Glass mat{};
            if (g.params.glass.has_value()) {
                mat = *g.params.glass;
            } else {
                for (const auto& c : g.children) {
                    if (c && c->params.glass.has_value()) {
                        mat = *c->params.glass;
                        break;
                    }
                }
            }
            GlassPill pills[kMaxGlassPills];
            const int n = collectGlassPills(g, pills);
            if (n > 0) {
                const PlateSpec field = glassPillsToField(pills, n, mat.mergeKPx, pr, surface);
                sampleGlassPlate(tmp, iw, ih, dest, w, h, xf, mat, field);
            }
        } else {
            const float sigma = snapBackdropSigma(g.params.backdropBlur);
            if (sigma > 0.f || g.params.backdropBend > 0.f) {
                if (g.params.backdropBend > 0.f) {
                    samplePlate(tmp, iw, ih, dest, w, h, xf, specFromGroup(g, surface, pr));
                } else {
                    PlateSpec copy = specFromGroup(g, surface, pr);
                    copyDestPlate(tmp, iw, ih, dest, w, h, xf);
                    if (sigma > 0.f) {
                        blurCheap(tmp, iw, ih, sigma * pr * 0.5f);
                    }
                    maskFrostPlate(tmp, iw, ih, copy);
                }
            }
        }
        auto local = cloneGroup(g);
        local->params.opacity = 1.0f;
        local->params.isolate = false;
        clearBackdropParams(local->params);
        local->params.transform = Mat4::identity();
        if (backdrop) {
            dropBackdropPills(*local);
        }
        if (glassWork) {
            stripGlassForIsolate(*local);
        }
        if (!hasClip(local->params) && surface.size.x > 0.f && surface.size.y > 0.f) {
            local->params.clip = Rect{{0, 0}, surface.size};
        }
        const Mat4 localX = Mat4::scale(pr, pr) * Mat4::translate(-surface.origin.x, -surface.origin.y);
        paintShapes(tmp, iw, ih, *local, images, localX, clipOf(local->params, localX), pr);
        blitBuffer(dest, w, h, tmp, iw, ih, xf, g.params.opacity);
        return;
    }
    paintShapes(dest, w, h, g, images, world, clip, pixelRatio);
}

void rasterIsolate(std::vector<Pixel>& dest, int w, int h, const Isolate& iso, const ImageStore& images,
                   float pixelRatio);

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
                const ImageStore& images, float pixelRatio) {
    for (const Quad& q : quads) {
        fillQuad(dest, w, h, q);
    }
    paintBlits(dest, w, h, blits, images);
    for (const Isolate& iso : isolates) {
        rasterIsolate(dest, w, h, iso, images, pixelRatio);
    }
}

void rasterIsolate(std::vector<Pixel>& dest, int w, int h, const Isolate& iso, const ImageStore& images,
                   float pixelRatio) {
    std::vector<Pixel> tmp(static_cast<std::size_t>(iso.contentW * iso.contentH), Pixel{0, 0, 0, 0});
    if (iso.hasGlass) {
        if (!iso.glassPills.empty()) {
            const Rect destR{{iso.destX, iso.destY}, {iso.destW, iso.destH}};
            const float pr = iso.destW > 0.f ? static_cast<float>(iso.contentW) / iso.destW : pixelRatio;
            const Rect local{{0.f, 0.f}, {iso.destW, iso.destH}};
            const PlateSpec field = glassPillsToField(
                iso.glassPills.empty() ? nullptr : iso.glassPills.data(),
                static_cast<int>(iso.glassPills.size()), iso.glass.mergeKPx, pr, local);
            sampleGlassPlate(tmp, iso.contentW, iso.contentH, dest, w, h, destR, iso.glass, field);
        }
    } else if (iso.backdropSigma > 0.f || iso.backdropBend > 0.f) {
        const Rect destR{{iso.destX, iso.destY}, {iso.destW, iso.destH}};
        const float pr = iso.destW > 0.f ? static_cast<float>(iso.contentW) / iso.destW : pixelRatio;
        if (iso.backdropBend > 0.f) {
            samplePlate(tmp, iso.contentW, iso.contentH, dest, w, h, destR, specFromIsolate(iso, pr));
        } else {
            PlateSpec copy = specFromIsolate(iso, pr);
            copyDestPlate(tmp, iso.contentW, iso.contentH, dest, w, h, destR);
            if (iso.backdropSigma > 0.f) {
                blurCheap(tmp, iso.contentW, iso.contentH, iso.backdropSigma * pixelRatio * 0.5f);
            }
            maskFrostPlate(tmp, iso.contentW, iso.contentH, copy);
        }
    }
    paintQuads(tmp, iso.contentW, iso.contentH, iso.quads, iso.blits, iso.isolates, images, pixelRatio);
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

void rasterScene(const Scene& scene, int width, int height, std::uint8_t* rgba, RasterOptions options) {
    tlsRaster = options;
    thread_local std::vector<Pixel> buf;
    buf.assign(static_cast<std::size_t>(width * height), Pixel{0, 0, 0, 1});
    Group root = merge(scene.root, nullptr);
    const float pr = scene.logicalSize.x > 0.f ? static_cast<float>(width) / scene.logicalSize.x : 1.f;
    rasterGroup(buf, width, height, root, scene.images, Mat4::identity(), {}, pr);
    toRgba(buf, rgba);
}

void rasterPacket(const FramePacket& packet, int width, int height, std::uint8_t* rgba,
                  RasterOptions options) {
    tlsRaster = options;
    thread_local std::vector<Pixel> buf;
    buf.assign(static_cast<std::size_t>(width * height), Pixel{0, 0, 0, 1});
    const float pr = packet.logicalSize.x > 0.f ? static_cast<float>(width) / packet.logicalSize.x : 1.f;
    paintQuads(buf, width, height, packet.quads, packet.blits, packet.isolates, packet.images, pr);
    toRgba(buf, rgba);
}

}  // namespace glim::paint
