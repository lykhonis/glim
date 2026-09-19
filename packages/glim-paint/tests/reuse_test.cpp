// Phase 2: Renderer-internal raster reuse (no app cache API).
//
// ReuseCache::encode must: hit on identical scenes, hit with rewritten
// origins on translation-only changes, miss on content changes, and never
// store scenes with foreign (volatile) Matter.

#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>
#include <glim/paint/Reuse.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

bool nearly(float a, float b) {
    return std::fabs(a - b) <= 1e-4f;
}

bool quadsEqual(const std::vector<glim::paint::Quad>& a, const std::vector<glim::paint::Quad>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!nearly(a[i].x, b[i].x) || !nearly(a[i].y, b[i].y) || !nearly(a[i].w, b[i].w) ||
            !nearly(a[i].h, b[i].h) || !nearly(a[i].r, b[i].r) || !nearly(a[i].g, b[i].g) ||
            !nearly(a[i].b, b[i].b) || !nearly(a[i].a, b[i].a)) {
            return false;
        }
    }
    return true;
}

bool isolatesEqual(const std::vector<glim::paint::Isolate>& a,
                   const std::vector<glim::paint::Isolate>& b);

bool packetsEqual(const glim::paint::FramePacket& a, const glim::paint::FramePacket& b) {
    if (a.quads.size() != b.quads.size() || a.blits.size() != b.blits.size() ||
        a.isolates.size() != b.isolates.size()) {
        return false;
    }
    if (!quadsEqual(a.quads, b.quads)) {
        return false;
    }
    for (std::size_t i = 0; i < a.blits.size(); ++i) {
        const auto& x = a.blits[i];
        const auto& y = b.blits[i];
        if (!nearly(x.x, y.x) || !nearly(x.y, y.y) || !nearly(x.w, y.w) || !nearly(x.h, y.h) ||
            x.imageId != y.imageId) {
            return false;
        }
    }
    return isolatesEqual(a.isolates, b.isolates);
}

bool isolatesEqual(const std::vector<glim::paint::Isolate>& a,
                   const std::vector<glim::paint::Isolate>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto& x = a[i];
        const auto& y = b[i];
        if (!nearly(x.destX, y.destX) || !nearly(x.destY, y.destY) || !nearly(x.destW, y.destW) ||
            !nearly(x.destH, y.destH) || x.contentW != y.contentW || x.contentH != y.contentH ||
            !nearly(x.backdropSigma, y.backdropSigma) || !nearly(x.backdropU0, y.backdropU0) ||
            !nearly(x.backdropV0, y.backdropV0) || !nearly(x.backdropU1, y.backdropU1) ||
            !nearly(x.backdropV1, y.backdropV1) || x.hasGlass != y.hasGlass ||
            !nearly(x.glassU0, y.glassU0) || !nearly(x.glassU1, y.glassU1)) {
            return false;
        }
        if (!quadsEqual(x.quads, y.quads) || !isolatesEqual(x.isolates, y.isolates)) {
            return false;
        }
    }
    return true;
}

void recordCards(glim::paint::Context& ctx, std::uint32_t color, float dx = 0.f, float dy = 0.f) {
    ctx.setSize({200, 120});
    ctx.beginFrame();
    ctx.translate({dx, dy});
    ctx.setFillColor(0x111111ff);
    ctx.fill(glim::Rect::fromSize({200, 120}));
    ctx.setFillColor(color);
    for (int i = 0; i < 8; ++i) {
        ctx.fill(glim::Rect{{8.f + static_cast<float>(i) * 24.f, 8.f}, {20, 40}});
    }
    ctx.fillRounded(glim::Rect{{8, 56}, {184, 48}}, glim::Radius{8.f});
    glim::paint::GroupParams fade;
    fade.opacity = 0.5f;
    fade.bounds = glim::Rect{{8, 56}, {184, 48}};
    ctx.pushGroup(fade);
    ctx.setFillColor(0xffffffff);
    ctx.fill(glim::Rect{{8, 56}, {184, 48}});
    ctx.popGroup();
    ctx.finish();
}

void recordFrost(glim::paint::Context& ctx, float dx = 0.f, float dy = 0.f) {
    ctx.setSize({120, 80});
    ctx.beginFrame();
    ctx.translate({dx, dy});
    ctx.setFillColor(0xff0000ff);
    ctx.fill(glim::Rect::fromSize({120, 80}));
    glim::paint::GroupParams frost;
    frost.backdropBlur = 8.f;
    frost.bounds = glim::Rect{{10, 10}, {60, 40}};
    ctx.pushGroup(frost);
    ctx.setFillColor(0xffffff66);
    ctx.fill(glim::Rect{{10, 10}, {60, 40}});
    ctx.popGroup();
    ctx.finish();
}

}  // namespace

int main() {
    // 1. Identical scenes hit.
    {
        glim::paint::ReuseCache cache;
        glim::paint::Context a;
        recordCards(a, 0x00ff00ff);
        const glim::paint::FramePacket first = cache.encode(a.scene(), 1.f);
        expect(first.stats.reuseMisses == 1 && first.stats.reuseHits == 0, "first encode misses");
        expect(cache.size() == 1, "miss stores one entry");
        glim::paint::Context b;
        recordCards(b, 0x00ff00ff);
        const glim::paint::FramePacket second = cache.encode(b.scene(), 1.f);
        expect(second.stats.reuseHits == 1 && second.stats.reuseMisses == 0, "identical scene hits");
        expect(packetsEqual(first, second), "hit returns identical packet");
    }

    // 2. Translation-only scene hits with rewritten origins.
    {
        glim::paint::ReuseCache cache;
        glim::paint::Context a;
        recordCards(a, 0x00ff00ff);
        const glim::paint::FramePacket first = cache.encode(a.scene(), 1.f);
        glim::paint::Context b;
        recordCards(b, 0x00ff00ff, 12.f, -7.f);
        const glim::paint::FramePacket moved = cache.encode(b.scene(), 1.f);
        expect(moved.stats.reuseHits == 1, "translation-only scene hits");
        const glim::paint::FramePacket fresh = glim::paint::encode(b.scene(), 1.f);
        expect(packetsEqual(moved, fresh), "translated hit matches fresh encode");
        expect(!moved.quads.empty() && nearly(moved.quads[0].x, first.quads[0].x + 12.f) &&
                   nearly(moved.quads[0].y, first.quads[0].y - 7.f),
               "translated hit rewrites quad origins");
    }

    // 3. Content change misses.
    {
        glim::paint::ReuseCache cache;
        glim::paint::Context a;
        recordCards(a, 0x00ff00ff);
        cache.encode(a.scene(), 1.f);
        glim::paint::Context b;
        recordCards(b, 0xff0000ff);
        const glim::paint::FramePacket changed = cache.encode(b.scene(), 1.f);
        expect(changed.stats.reuseMisses == 1 && changed.stats.reuseHits == 0,
               "color change misses");
    }

    // 4. Foreign Matter never stores.
    {
        glim::paint::ReuseCache cache;
        glim::paint::Context a;
        a.setSize({64, 64});
        a.beginFrame();
        a.setFillColor(0xff0000ff);
        a.fill(glim::Rect::fromSize({64, 64}));
        const std::uint32_t foreignId =
            a.wrapNativeTexture(reinterpret_cast<void*>(0x1234), 16, 16);
        a.blit(glim::Rect{{8, 8}, {16, 16}},
               glim::paint::Matter::foreign(foreignId, glim::Color{0xffffffff}));
        a.finish();
        const glim::paint::FramePacket first = cache.encode(a.scene(), 1.f);
        expect(first.stats.reuseMisses == 1, "foreign scene encodes");
        expect(cache.size() == 0, "foreign scene is never stored");
        const glim::paint::FramePacket second = cache.encode(a.scene(), 1.f);
        expect(second.stats.reuseMisses == 1 && second.stats.reuseHits == 0,
               "foreign scene always misses");
    }

    // 5. Backdrop isolate survives translation with correct UVs.
    {
        glim::paint::ReuseCache cache;
        glim::paint::Context a;
        recordFrost(a);
        const glim::paint::FramePacket first = cache.encode(a.scene(), 1.f);
        expect(first.isolates.size() == 1, "frost encodes one isolate");
        glim::paint::Context b;
        recordFrost(b, 5.f, 3.f);
        const glim::paint::FramePacket moved = cache.encode(b.scene(), 1.f);
        expect(moved.stats.reuseHits == 1, "translated frost hits");
        const glim::paint::FramePacket fresh = glim::paint::encode(b.scene(), 1.f);
        expect(packetsEqual(moved, fresh), "translated frost matches fresh encode");
        expect(moved.isolates.size() == 1 &&
                   nearly(moved.isolates[0].backdropU0, moved.isolates[0].destX / 120.f),
               "isolate UVs rebase to the new dest");
    }

    // 6. clear() forces a miss.
    {
        glim::paint::ReuseCache cache;
        glim::paint::Context a;
        recordCards(a, 0x00ff00ff);
        cache.encode(a.scene(), 1.f);
        cache.clear();
        expect(cache.size() == 0, "clear empties the cache");
        const glim::paint::FramePacket after = cache.encode(a.scene(), 1.f);
        expect(after.stats.reuseMisses == 1, "encode after clear misses");
    }

    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "reuse_test ok\n";
    return EXIT_SUCCESS;
}
