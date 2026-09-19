#include <glim/paint/Reuse.h>

#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace glim::paint {
namespace {

// FNV-1a 64.
constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

struct Hasher {
    std::uint64_t h = kFnvOffset;
    void u32(std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            h ^= static_cast<std::uint64_t>((v >> (i * 8)) & 0xffu);
            h *= kFnvPrime;
        }
    }
    void u64(std::uint64_t v) {
        u32(static_cast<std::uint32_t>(v & 0xffffffffull));
        u32(static_cast<std::uint32_t>(v >> 32));
    }
    void f32(float v) {
        std::uint32_t b = 0;
        std::memcpy(&b, &v, sizeof(b));
        u32(b);
    }
    void boolean(bool v) { u32(v ? 1u : 0u); }
    void bytes(const void* data, std::size_t n) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        for (std::size_t i = 0; i < n; ++i) {
            h ^= static_cast<std::uint64_t>(p[i]);
            h *= kFnvPrime;
        }
    }
};

// Bucket pixelRatio so tiny jitter does not thrash the cache.
float bucketPixelRatio(float pr) {
    if (!(pr > 0.f)) {
        return 1.f;
    }
    return std::floor(pr * 32.f + 0.5f) / 32.f;
}

struct FpContext {
    Hasher h;
    Vec2 origin{};
    bool hasForeign = false;
    bool has3D = false;
    // Fingerprints of sampled images, keyed by imageId. Slots are
    // append-only and add() never mutates a stored image, so id->content is
    // immutable within a store; entries are validated against the live slot
    // (pointer + dimensions) so ids from a different ImageStore never alias.
    struct ImgFp {
        const StoredImage* ptr = nullptr;
        int w = 0;
        int h = 0;
        std::size_t bytes = 0;
        std::uint64_t fp = 0;
    };
    std::unordered_map<std::uint32_t, ImgFp> imageFp;
    const ImageStore* images = nullptr;
};

std::uint64_t fingerprintSampled(const StoredImage& img) {
    Hasher h;
    h.u32(static_cast<std::uint32_t>(img.width));
    h.u32(static_cast<std::uint32_t>(img.height));
    h.u64(static_cast<std::uint64_t>(img.rgba.size()));
    if (!img.rgba.empty()) {
        h.bytes(img.rgba.data(), img.rgba.size());
    }
    return h.h;
}

void hashImageRef(FpContext& ctx, std::uint32_t imageId) {
    ctx.h.u32(imageId);
    if (imageId == 0 || !ctx.images) {
        return;
    }
    const StoredImage* img = ctx.images->get(imageId);
    if (!img) {
        return;
    }
    if (img->foreign) {
        // Volatile (video): identity includes the native handle, and the
        // scene is marked uncacheable below.
        ctx.hasForeign = true;
        ctx.h.u64(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(img->native)));
        ctx.h.u32(static_cast<std::uint32_t>(img->width));
        ctx.h.u32(static_cast<std::uint32_t>(img->height));
        ctx.h.u32(static_cast<std::uint32_t>(img->format));
        return;
    }
    auto it = ctx.imageFp.find(imageId);
    if (it != ctx.imageFp.end() && it->second.ptr == img && it->second.w == img->width &&
        it->second.h == img->height && it->second.bytes == img->rgba.size()) {
        ctx.h.u64(it->second.fp);
        return;
    }
    FpContext::ImgFp entry;
    entry.ptr = img;
    entry.w = img->width;
    entry.h = img->height;
    entry.bytes = img->rgba.size();
    entry.fp = fingerprintSampled(*img);
    ctx.imageFp[imageId] = entry;
    ctx.h.u64(entry.fp);
}

void hashMatter(FpContext& ctx, const Matter& m) {
    ctx.h.u32(static_cast<std::uint32_t>(m.kind));
    ctx.h.u32(m.color.rgba);
    ctx.h.f32(m.uv.origin.x);
    ctx.h.f32(m.uv.origin.y);
    ctx.h.f32(m.uv.size.x);
    ctx.h.f32(m.uv.size.y);
    hashImageRef(ctx, m.imageId);
}

void hashRadius(FpContext& ctx, const Radius& r) {
    ctx.h.f32(r.lt);
    ctx.h.f32(r.rt);
    ctx.h.f32(r.lb);
    ctx.h.f32(r.rb);
}

void hashShape(FpContext& ctx, const Shape& s, bool normalize);

void hashGroup(FpContext& ctx, const Group& g, int windowLevel);

// Window-space origin for translation normalization. Only root-level
// quantities live in window space: root shapes (baked at record), the root
// bounds/clip, and direct-child transform translations (local->window).
// Deeper levels are child-local and never move under a whole-scene shift,
// so they hash raw and must not anchor the origin.
Vec2 windowOrigin(const Group& root) {
    float x = std::numeric_limits<float>::infinity();
    float y = std::numeric_limits<float>::infinity();
    const auto consider = [&](float px, float py) {
        if (px < x) {
            x = px;
        }
        if (py < y) {
            y = py;
        }
    };
    const auto considerRect = [&](const Rect& r) {
        if (r.size.x > 0.f && r.size.y > 0.f) {
            consider(r.origin.x, r.origin.y);
        }
    };
    considerRect(root.params.bounds);
    if (hasClip(root.params)) {
        considerRect(root.params.clip);
    }
    for (const Shape& s : root.shapes) {
        if (const auto* f = std::get_if<FillRect>(&s)) {
            considerRect(f->rect);
        } else if (const auto* r = std::get_if<FillRounded>(&s)) {
            considerRect(r->rect);
        } else if (const auto* st = std::get_if<Stroke>(&s)) {
            considerRect(st->rect);
        } else if (const auto* b = std::get_if<Blit>(&s)) {
            considerRect(b->rect);
        } else if (const auto* run = std::get_if<GlyphRun>(&s)) {
            consider(run->origin.x, run->origin.y);
            for (const GlyphQuad& q : run->glyphs) {
                considerRect(q.dest);
            }
        } else if (const auto* hole = std::get_if<SlotHole>(&s)) {
            considerRect(hole->rect);
        }
    }
    for (const auto& child : root.children) {
        if (child) {
            consider(child->params.transform.m[12], child->params.transform.m[13]);
        }
    }
    if (x == std::numeric_limits<float>::infinity()) {
        return {};
    }
    return {x, y};
}

void hashSceneInto(FpContext& ctx, const Scene& scene, float pixelRatio) {
    ctx.images = &scene.images;
    ctx.origin = windowOrigin(scene.root);
    ctx.h.f32(scene.logicalSize.x);
    ctx.h.f32(scene.logicalSize.y);
    ctx.h.f32(bucketPixelRatio(pixelRatio));
    hashGroup(ctx, scene.root, 0);
}
void hashRect(FpContext& ctx, const Rect& r, bool normalize) {
    // Only non-empty rects carry position. An empty rect is "unset"
    // (bounds/clip default); normalizing its origin would inject the scene
    // translation into the key and break translation-only hits.
    const bool placed = normalize && r.size.x > 0.f && r.size.y > 0.f;
    ctx.h.f32(placed ? r.origin.x - ctx.origin.x : r.origin.x);
    ctx.h.f32(placed ? r.origin.y - ctx.origin.y : r.origin.y);
    ctx.h.f32(r.size.x);
    ctx.h.f32(r.size.y);
}

void hashPoint(FpContext& ctx, Vec2 p, bool normalize) {
    ctx.h.f32(normalize ? p.x - ctx.origin.x : p.x);
    ctx.h.f32(normalize ? p.y - ctx.origin.y : p.y);
}

// Column-major Mat4: m[12], m[13] are the XY translation.
void hashTransform(FpContext& ctx, const Mat4& t, bool normalize) {
    if (t.is3D()) {
        ctx.has3D = true;
    }
    for (int i = 0; i < 16; ++i) {
        float v = t.m[i];
        if (normalize) {
            if (i == 12) {
                v -= ctx.origin.x;
            } else if (i == 13) {
                v -= ctx.origin.y;
            }
        }
        ctx.h.f32(v);
    }
}

void hashShape(FpContext& ctx, const Shape& s, bool normalize) {
    ctx.h.u32(static_cast<std::uint32_t>(s.index()));
    if (const auto* f = std::get_if<FillRect>(&s)) {
        hashRect(ctx, f->rect, normalize);
        hashMatter(ctx, f->matter);
    } else if (const auto* r = std::get_if<FillRounded>(&s)) {
        hashRect(ctx, r->rect, normalize);
        hashRadius(ctx, r->radius);
        hashMatter(ctx, r->matter);
    } else if (const auto* st = std::get_if<Stroke>(&s)) {
        hashRect(ctx, st->rect, normalize);
        hashRadius(ctx, st->radius);
        ctx.h.f32(st->width);
        hashMatter(ctx, st->matter);
    } else if (const auto* b = std::get_if<Blit>(&s)) {
        hashRect(ctx, b->rect, normalize);
        hashMatter(ctx, b->matter);
    } else if (const auto* run = std::get_if<GlyphRun>(&s)) {
        hashPoint(ctx, run->origin, normalize);
        ctx.h.f32(run->sizePx);
        ctx.h.u32(run->color.rgba);
        hashImageRef(ctx, run->imageId);
        ctx.h.u64(static_cast<std::uint64_t>(run->glyphs.size()));
        for (const GlyphQuad& q : run->glyphs) {
            hashRect(ctx, q.dest, normalize);
            ctx.h.f32(q.uv.origin.x);
            ctx.h.f32(q.uv.origin.y);
            ctx.h.f32(q.uv.size.x);
            ctx.h.f32(q.uv.size.y);
        }
    } else if (const auto* hole = std::get_if<SlotHole>(&s)) {
        hashRect(ctx, hole->rect, normalize);
        ctx.h.u32(hole->id);
    }
}

// windowLevel: 0 = root (window space: normalize content; the root transform
// itself never carries scene translation, so it hashes raw), 1 = direct
// child (transform maps local->window: normalize transform only; local
// content hashes raw), 2+ = deeper (child-local: hash raw; never moves under
// a whole-scene shift).
void hashGroup(FpContext& ctx, const Group& g, int windowLevel) {
    const GroupParams& p = g.params;
    const bool windowContent = windowLevel == 0;
    const bool windowTransform = windowLevel == 1;
    ctx.h.f32(p.opacity);
    hashTransform(ctx, p.transform, windowTransform);
    hashRect(ctx, p.bounds, windowContent);
    ctx.h.boolean(p.isolate);
    ctx.h.u32(static_cast<std::uint32_t>(p.blend));
    hashRect(ctx, p.clip, windowContent);
    hashRadius(ctx, p.clipRadius);
    ctx.h.f32(snapBackdropSigma(p.backdropBlur));
    ctx.h.f32(p.backdropBend);
    ctx.h.f32(p.backdropMerge);
    ctx.h.f32(p.backdropPress);
    ctx.h.f32(p.backdropLightX);
    ctx.h.f32(p.backdropLightY);
    ctx.h.f32(p.backdropLightZ);
    ctx.h.boolean(p.backdropFlat);
    ctx.h.boolean(p.glass.has_value());
    if (p.glass.has_value()) {
        const Glass& gl = *p.glass;
        ctx.h.u32(static_cast<std::uint32_t>(gl.variant));
        ctx.h.u32(gl.tint.rgba);
        ctx.h.boolean(gl.interactive);
        ctx.h.f32(gl.ior);
        ctx.h.f32(gl.thicknessPx);
        ctx.h.f32(gl.mergeKPx);
        ctx.h.f32(gl.refDistance);
        ctx.h.f32(gl.dispersion);
        ctx.h.boolean(gl.flatten);
    }
    ctx.h.boolean(p.glassContainer);
    ctx.h.u32(p.semantic);
    const int childLevel = windowLevel >= 2 ? 2 : windowLevel + 1;
    visitGroup(
        g, [&](const Shape& s) { hashShape(ctx, s, windowContent); },
        [&](const Group& child) { hashGroup(ctx, child, childLevel); });
}

// Whole-scene translation shifts top-level quads/blits and isolate dest
// rects. Isolate *contents* are surface-relative (encodeIsolate rebases by
// -surface.origin), so they are unchanged. Isolate dest UVs are
// window-relative and must be recomputed from the new dest.
void rebaseIsolateUv(Isolate& iso, Vec2 logicalSize) {
    if (logicalSize.x <= 0.f || logicalSize.y <= 0.f) {
        return;
    }
    if (iso.backdropSigma > 0.f || iso.backdropBend > 0.f) {
        iso.backdropU0 = iso.destX / logicalSize.x;
        iso.backdropV0 = iso.destY / logicalSize.y;
        iso.backdropU1 = (iso.destX + iso.destW) / logicalSize.x;
        iso.backdropV1 = (iso.destY + iso.destH) / logicalSize.y;
    }
    if (iso.hasGlass) {
        iso.glassU0 = iso.destX / logicalSize.x;
        iso.glassV0 = iso.destY / logicalSize.y;
        iso.glassU1 = (iso.destX + iso.destW) / logicalSize.x;
        iso.glassV1 = (iso.destY + iso.destH) / logicalSize.y;
    }
}

void offsetPacket(FramePacket& packet, float dx, float dy) {
    if (dx == 0.f && dy == 0.f) {
        return;
    }
    for (Quad& q : packet.quads) {
        q.x += dx;
        q.y += dy;
    }
    for (BlitQuad& q : packet.blits) {
        q.x += dx;
        q.y += dy;
    }
    for (Isolate& iso : packet.isolates) {
        iso.destX += dx;
        iso.destY += dy;
        rebaseIsolateUv(iso, packet.logicalSize);
    }
}

}  // namespace

struct ReuseCache::Impl {
    struct Entry {
        std::uint64_t hash = 0;
        Vec2 origin{};
        Vec2 logicalSize{};
        float prBucket = 1.f;
        bool has3D = false;
        FramePacket packet;
    };
    static constexpr std::size_t kMaxEntries = 8;
    std::deque<std::uint64_t> order;
    std::unordered_map<std::uint64_t, Entry> entries;
    std::unordered_map<std::uint32_t, FpContext::ImgFp> imageFp;
};

ReuseCache::ReuseCache() : impl_(std::make_unique<Impl>()) {}
ReuseCache::~ReuseCache() = default;

void ReuseCache::clear() {
    impl_->entries.clear();
    impl_->order.clear();
    impl_->imageFp.clear();
}

std::size_t ReuseCache::size() const {
    return impl_->entries.size();
}

std::uint64_t hashSceneStructure(const Scene& scene, float pixelRatio, Vec2* outOrigin,
                                 bool* outHasForeign, bool* outHas3D) {
    FpContext ctx;
    hashSceneInto(ctx, scene, pixelRatio);
    if (outOrigin) {
        *outOrigin = ctx.origin;
    }
    if (outHasForeign) {
        *outHasForeign = ctx.hasForeign;
    }
    if (outHas3D) {
        *outHas3D = ctx.has3D;
    }
    return ctx.h.h;
}

FramePacket ReuseCache::encode(const Scene& scene, float pixelRatio) {
    const auto t0 = std::chrono::steady_clock::now();
    // Seed the fingerprint cache with previously computed entries so
    // sampled-image bytes hash once per imageId (slots are append-only).
    FpContext probe;
    probe.imageFp = impl_->imageFp;
    hashSceneInto(probe, scene, pixelRatio);
    impl_->imageFp = probe.imageFp;

    const std::uint64_t hash = probe.h.h;
    const float prBucket = bucketPixelRatio(pixelRatio);
    const auto finish = [&](FramePacket pkt) {
        pkt.stats.encodeMs =
            std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
        return pkt;
    };

    auto it = impl_->entries.find(hash);
    if (it != impl_->entries.end() && !probe.hasForeign && !probe.has3D && !it->second.has3D &&
        it->second.logicalSize.x == scene.logicalSize.x &&
        it->second.logicalSize.y == scene.logicalSize.y && it->second.prBucket == prBucket) {
        FramePacket hit = it->second.packet;
        offsetPacket(hit, probe.origin.x - it->second.origin.x, probe.origin.y - it->second.origin.y);
        hit.stats.reuseHits = 1;
        hit.stats.reuseMisses = 0;
        hit.stats.dirtyTiles = 0;
        return finish(hit);
    }

    FramePacket fresh = glim::paint::encode(scene, pixelRatio);
    fresh.stats.reuseHits = 0;
    fresh.stats.reuseMisses = 1;
    fresh.stats.dirtyTiles = 0;
    if (!probe.hasForeign) {
        if (impl_->entries.size() >= Impl::kMaxEntries) {
            impl_->entries.erase(impl_->order.front());
            impl_->order.pop_front();
        }
        Impl::Entry entry;
        entry.hash = hash;
        entry.origin = probe.origin;
        entry.logicalSize = scene.logicalSize;
        entry.prBucket = prBucket;
        entry.has3D = probe.has3D;
        entry.packet = fresh;
        impl_->entries.emplace(hash, std::move(entry));
        impl_->order.push_back(hash);
    }
    return finish(fresh);
}

FramePacket encodeWithReuse(const Scene& scene, ReuseCache& cache, float pixelRatio) {
    return cache.encode(scene, pixelRatio);
}

}  // namespace glim::paint
