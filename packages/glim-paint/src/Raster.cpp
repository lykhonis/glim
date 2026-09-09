#include <glim/paint/Raster.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <zlib.h>

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

void encodeMerged(std::vector<Pixel>& dest, int w, int h, const Group& g) {
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
        encodeMerged(tmp, iw, ih, *local);
        const FillRect moved{{b.origin, b.size}, Color{}};
        const FillRect xf = transformFill(g.params.transform, moved);
        blitBuffer(dest, w, h, tmp, iw, ih, xf.rect, g.params.opacity);
        return;
    }
    encodeMerged(dest, w, h, g);
}

std::uint32_t crc32Png(const std::uint8_t* data, std::size_t n) {
    static std::uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        init = true;
    }
    std::uint32_t c = 0xffffffffu;
    for (std::size_t i = 0; i < n; ++i) {
        c = table[(c ^ data[i]) & 0xff] ^ (c >> 8);
    }
    return c ^ 0xffffffffu;
}

void put32(std::vector<std::uint8_t>& o, std::uint32_t v) {
    o.push_back(static_cast<std::uint8_t>(v >> 24));
    o.push_back(static_cast<std::uint8_t>(v >> 16));
    o.push_back(static_cast<std::uint8_t>(v >> 8));
    o.push_back(static_cast<std::uint8_t>(v));
}

void chunk(std::vector<std::uint8_t>& o, const char type[4], const std::uint8_t* data, std::size_t n) {
    put32(o, static_cast<std::uint32_t>(n));
    const std::size_t start = o.size();
    o.insert(o.end(), type, type + 4);
    o.insert(o.end(), data, data + n);
    const std::uint32_t crc = crc32Png(o.data() + start, 4 + n);
    put32(o, crc);
}

}  // namespace

void raster(const Scene& scene, int width, int height, std::uint8_t* rgba) {
    std::vector<Pixel> buf(static_cast<std::size_t>(width * height), Pixel{0, 0, 0, 1});
    Stats stats{};
    Group root = merge(scene.root, &stats);
    rasterGroup(buf, width, height, root);
    for (int i = 0; i < width * height; ++i) {
        rgba[i * 4 + 0] = static_cast<std::uint8_t>(std::round(std::min(1.f, buf[i].r) * 255.f));
        rgba[i * 4 + 1] = static_cast<std::uint8_t>(std::round(std::min(1.f, buf[i].g) * 255.f));
        rgba[i * 4 + 2] = static_cast<std::uint8_t>(std::round(std::min(1.f, buf[i].b) * 255.f));
        rgba[i * 4 + 3] = static_cast<std::uint8_t>(std::round(std::min(1.f, buf[i].a) * 255.f));
    }
}

bool writeRgbaPng(const char* path, int width, int height, const std::uint8_t* rgba) {
    std::vector<std::uint8_t> raw(static_cast<std::size_t>((width * 4 + 1) * height));
    for (int y = 0; y < height; ++y) {
        raw[static_cast<std::size_t>(y * (width * 4 + 1))] = 0;
        std::memcpy(&raw[static_cast<std::size_t>(y * (width * 4 + 1) + 1)], rgba + y * width * 4,
                    static_cast<std::size_t>(width * 4));
    }
    uLongf bound = compressBound(static_cast<uLong>(raw.size()));
    std::vector<std::uint8_t> defl(bound);
    if (compress2(defl.data(), &bound, raw.data(), static_cast<uLong>(raw.size()), Z_BEST_COMPRESSION) != Z_OK) {
        return false;
    }
    defl.resize(bound);

    std::vector<std::uint8_t> png;
    const std::uint8_t sig[] = {137, 80, 78, 71, 13, 10, 26, 10};
    png.insert(png.end(), sig, sig + 8);
    std::uint8_t ihdr[13]{};
    ihdr[0] = static_cast<std::uint8_t>(width >> 24);
    ihdr[1] = static_cast<std::uint8_t>(width >> 16);
    ihdr[2] = static_cast<std::uint8_t>(width >> 8);
    ihdr[3] = static_cast<std::uint8_t>(width);
    ihdr[4] = static_cast<std::uint8_t>(height >> 24);
    ihdr[5] = static_cast<std::uint8_t>(height >> 16);
    ihdr[6] = static_cast<std::uint8_t>(height >> 8);
    ihdr[7] = static_cast<std::uint8_t>(height);
    ihdr[8] = 8;
    ihdr[9] = 6;
    chunk(png, "IHDR", ihdr, 13);
    chunk(png, "IDAT", defl.data(), defl.size());
    chunk(png, "IEND", nullptr, 0);

    FILE* f = std::fopen(path, "wb");
    if (!f) {
        return false;
    }
    const bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
    std::fclose(f);
    return ok;
}

bool readRgbaPng(const char* path, int* width, int* height, std::vector<std::uint8_t>* rgba) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<std::uint8_t> png(static_cast<std::size_t>(n));
    if (n <= 0 || std::fread(png.data(), 1, png.size(), f) != png.size()) {
        std::fclose(f);
        return false;
    }
    std::fclose(f);
    if (png.size() < 8 || png[0] != 137) {
        return false;
    }
    std::size_t i = 8;
    int w = 0;
    int h = 0;
    std::vector<std::uint8_t> idat;
    while (i + 8 <= png.size()) {
        const std::uint32_t len = (std::uint32_t(png[i]) << 24) | (std::uint32_t(png[i + 1]) << 16) |
                                  (std::uint32_t(png[i + 2]) << 8) | png[i + 3];
        i += 4;
        if (i + 4 + len + 4 > png.size()) {
            return false;
        }
        const char* type = reinterpret_cast<const char*>(&png[i]);
        i += 4;
        if (std::memcmp(type, "IHDR", 4) == 0 && len >= 13) {
            w = (png[i] << 24) | (png[i + 1] << 16) | (png[i + 2] << 8) | png[i + 3];
            h = (png[i + 4] << 24) | (png[i + 5] << 16) | (png[i + 6] << 8) | png[i + 7];
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), png.begin() + static_cast<std::ptrdiff_t>(i),
                        png.begin() + static_cast<std::ptrdiff_t>(i + len));
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        i += len + 4;
    }
    if (w <= 0 || h <= 0 || idat.empty()) {
        return false;
    }
    const std::size_t rawSize = static_cast<std::size_t>((w * 4 + 1) * h);
    std::vector<std::uint8_t> raw(rawSize);
    uLongf dest = static_cast<uLongf>(rawSize);
    if (uncompress(raw.data(), &dest, idat.data(), static_cast<uLong>(idat.size())) != Z_OK) {
        return false;
    }
    rgba->assign(static_cast<std::size_t>(w * h * 4), 0);
    for (int y = 0; y < h; ++y) {
        if (raw[static_cast<std::size_t>(y * (w * 4 + 1))] != 0) {
            return false;
        }
        std::memcpy(rgba->data() + y * w * 4, &raw[static_cast<std::size_t>(y * (w * 4 + 1) + 1)],
                    static_cast<std::size_t>(w * 4));
    }
    *width = w;
    *height = h;
    return true;
}

}  // namespace glim::paint
