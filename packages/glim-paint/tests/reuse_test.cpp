// Phase 2: Renderer-internal raster reuse (hits, translation hits, content misses, no foreign
// storage).

#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>
#include <glim/paint/Reuse.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

bool nearly(float a, float b) { return std::fabs(a - b) <= 1e-4f; }

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

bool isolatesEqual(
    const std::vector<glim::paint::Isolate>& a, const std::vector<glim::paint::Isolate>& b
);

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

bool isolatesEqual(
    const std::vector<glim::paint::Isolate>& a, const std::vector<glim::paint::Isolate>& b
) {
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

void recordGlassAt(glim::paint::Context& ctx, glim::paint::Lens glass) {
  ctx.setSize({200, 120});
  ctx.beginFrame();
  ctx.setFillColor(0x1a2228ff);
  ctx.fill(glim::Rect::fromSize({200, 120}));
  glim::paint::GroupParams gp;
  gp.bounds = glim::Rect{{20, 20}, {160, 80}};
  gp.clip = glim::Rect{{20, 20}, {160, 80}};
  gp.lens = glass;
  ctx.pushGroup(gp);
  ctx.setFillColor(0xffffffff);
  ctx.fill(glim::Rect{{20, 20}, {160, 80}});
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
    expect(
        !moved.quads.empty() && nearly(moved.quads[0].x, first.quads[0].x + 12.f) &&
            nearly(moved.quads[0].y, first.quads[0].y - 7.f),
        "translated hit rewrites quad origins"
    );
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
    expect(changed.stats.reuseMisses == 1 && changed.stats.reuseHits == 0, "color change misses");
  }

  // 4. Foreign Matter never stores.
  {
    glim::paint::ReuseCache cache;
    glim::paint::Context a;
    a.setSize({64, 64});
    a.beginFrame();
    a.setFillColor(0xff0000ff);
    a.fill(glim::Rect::fromSize({64, 64}));
    const std::uint32_t foreignId = a.wrapNativeTexture(reinterpret_cast<void*>(0x1234), 16, 16);
    a.blit(
        glim::Rect{{8, 8}, {16, 16}},
        glim::paint::Matter::foreign(foreignId, glim::Color{0xffffffff})
    );
    a.finish();
    const glim::paint::FramePacket first = cache.encode(a.scene(), 1.f);
    expect(first.stats.reuseMisses == 1, "foreign scene encodes");
    expect(cache.size() == 0, "foreign scene is never stored");
    const glim::paint::FramePacket second = cache.encode(a.scene(), 1.f);
    expect(
        second.stats.reuseMisses == 1 && second.stats.reuseHits == 0, "foreign scene always misses"
    );
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
    expect(
        moved.isolates.size() == 1 &&
            nearly(moved.isolates[0].backdropU0, moved.isolates[0].destX / 120.f),
        "isolate UVs rebase to the new dest"
    );
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

  // 7. Tile dirty tracking: identical frames are clean.
  {
    glim::paint::ReuseCache cache;
    glim::paint::Context a;
    recordCards(a, 0x00ff00ff);
    const glim::paint::FramePacket first = cache.encode(a.scene(), 1.f);
    expect(first.stats.dirtyTiles == 28, "first frame dirties all 7x4 tiles");
    glim::paint::Context b;
    recordCards(b, 0x00ff00ff);
    const glim::paint::FramePacket second = cache.encode(b.scene(), 1.f);
    expect(
        second.stats.reuseHits == 1 && second.stats.dirtyTiles == 0,
        "identical frame hits with zero dirty tiles"
    );
  }

  // 8. One small content change dirties only nearby tiles.
  {
    glim::paint::ReuseCache cache;
    glim::paint::Context a;
    a.setSize({200, 120});
    a.beginFrame();
    a.setFillColor(0x111111ff);
    a.fill(glim::Rect::fromSize({200, 120}));
    a.setFillColor(0x00ff00ff);
    a.fill(glim::Rect{{8, 8}, {20, 40}});
    a.finish();
    cache.encode(a.scene(), 1.f);
    glim::paint::Context b;
    b.setSize({200, 120});
    b.beginFrame();
    b.setFillColor(0x111111ff);
    b.fill(glim::Rect::fromSize({200, 120}));
    b.setFillColor(0xff0000ff);
    b.fill(glim::Rect{{8, 8}, {20, 40}});
    b.finish();
    const glim::paint::FramePacket changed = cache.encode(b.scene(), 1.f);
    expect(changed.stats.reuseMisses == 1, "color change misses");
    expect(
        changed.stats.dirtyTiles > 0 && changed.stats.dirtyTiles <= 6,
        "small change dirties only nearby tiles"
    );
  }

  // 9. Whole-scene translation dirties overlapped tiles but still hits.
  {
    glim::paint::ReuseCache cache;
    glim::paint::Context a;
    recordCards(a, 0x00ff00ff);
    cache.encode(a.scene(), 1.f);
    glim::paint::Context b;
    recordCards(b, 0x00ff00ff, 64.f, 0.f);
    const glim::paint::FramePacket moved = cache.encode(b.scene(), 1.f);
    expect(moved.stats.reuseHits == 1, "translated scene hits");
    expect(moved.stats.dirtyTiles > 0, "moved content dirties tiles");
  }

  {
    glim::paint::ReuseCache cache;
    glim::paint::Context a;
    recordCards(a, 0x00ff00ff);
    cache.encode(a.scene(), 1.f);
    glim::paint::Context b;
    recordCards(b, 0x00ff00ff);
    const glim::paint::FramePacket second = cache.encode(b.scene(), 1.f);
    expect(second.stats.dirtyTiles == 0, "identical frame has zero dirty tiles");
    expect(
        second.dirtyRect.size.x <= 0.f || second.dirtyRect.size.y <= 0.f,
        "identical frame has empty dirty rect"
    );
  }

  {
    glim::paint::ReuseCache cache;
    glim::paint::Context a;
    a.setSize({200, 120});
    a.beginFrame();
    a.setFillColor(0x111111ff);
    a.fill(glim::Rect::fromSize({200, 120}));
    a.setFillColor(0x00ff00ff);
    a.fill(glim::Rect{{8, 8}, {20, 40}});
    a.finish();
    cache.encode(a.scene(), 1.f);
    glim::paint::Context b;
    b.setSize({200, 120});
    b.beginFrame();
    b.setFillColor(0x111111ff);
    b.fill(glim::Rect::fromSize({200, 120}));
    b.setFillColor(0xff0000ff);
    b.fill(glim::Rect{{8, 8}, {20, 40}});
    b.finish();
    const glim::paint::FramePacket changed = cache.encode(b.scene(), 1.f);
    expect(changed.stats.dirtyTiles > 0, "color change dirties tiles");
    expect(
        changed.dirtyRect.size.x > 0.f && changed.dirtyRect.size.y > 0.f,
        "color change has non-empty dirty rect"
    );
    expect(
        changed.dirtyRect.origin.x >= 0.f && changed.dirtyRect.origin.y >= 0.f,
        "dirty rect stays in scene bounds"
    );
    expect(
        changed.dirtyRect.origin.x + changed.dirtyRect.size.x <= 200.f &&
            changed.dirtyRect.origin.y + changed.dirtyRect.size.y <= 120.f,
        "dirty rect stays inside the scene"
    );
    expect(
        changed.dirtyRect.origin.x < 64.f && changed.dirtyRect.origin.y < 64.f,
        "dirty rect covers the changed card"
    );
  }

  // 10. Cached structural hash matches the uncached hash; fingerprints memoized.
  {
    glim::paint::Context a;
    a.setSize({64, 64});
    a.beginFrame();
    std::vector<std::uint8_t> px(4 * 4 * 4, 128);
    const std::uint32_t img = a.addImage(4, 4, px.data());
    a.blit(glim::Rect{{8, 8}, {16, 16}}, glim::paint::Matter::sampled(img));
    a.finish();
    const std::uint64_t plain = glim::paint::hashSceneStructure(a.scene(), 1.f);
    glim::paint::SceneHashCache cache;
    const std::uint64_t first = glim::paint::hashSceneStructure(a.scene(), 1.f, cache);
    expect(first == plain, "cached hash matches uncached hash");
    expect(!cache.images.empty(), "cached hash memoizes image fingerprints");
    const std::uint64_t second = glim::paint::hashSceneStructure(a.scene(), 1.f, cache);
    expect(second == plain, "memoized hash stays stable across frames");
  }

  // 11. Fingerprint cache never aliases ids across ImageStores.
  {
    auto recordSwatch = [](glim::paint::Context& ctx, std::uint8_t v) {
      ctx.setSize({64, 64});
      ctx.beginFrame();
      std::vector<std::uint8_t> px(2 * 2 * 4, v);
      const std::uint32_t img = ctx.addImage(2, 2, px.data());
      ctx.blit(glim::Rect{{8, 8}, {16, 16}}, glim::paint::Matter::sampled(img));
      ctx.finish();
    };
    glim::paint::Context a;
    recordSwatch(a, 10);
    glim::paint::Context b;
    recordSwatch(b, 200);
    glim::paint::SceneHashCache cache;
    const std::uint64_t ha = glim::paint::hashSceneStructure(a.scene(), 1.f, cache);
    const std::uint64_t hb = glim::paint::hashSceneStructure(b.scene(), 1.f, cache);
    expect(ha != hb, "same image id with different bytes hashes differently");
    const std::uint64_t plainB = glim::paint::hashSceneStructure(b.scene(), 1.f);
    expect(hb == plainB, "shared cache does not corrupt later hashes");
  }

  {
    auto hashOf = [](const std::vector<std::uint8_t>& px) {
      glim::paint::Context ctx;
      ctx.setSize({64, 64});
      ctx.beginFrame();
      const std::uint32_t img = ctx.addImage(5, 3, px.data());
      ctx.blit(glim::Rect{{8, 8}, {16, 16}}, glim::paint::Matter::sampled(img));
      ctx.finish();
      return glim::paint::hashSceneStructure(ctx.scene(), 1.f);
    };
    const std::vector<std::uint8_t> base(5 * 3 * 4, 77);
    const std::uint64_t h0 = hashOf(base);
    for (const std::size_t at :
         {std::size_t{0}, std::size_t{31}, std::size_t{32}, std::size_t{59}}) {
      std::vector<std::uint8_t> px = base;
      px[at] ^= 0x80;
      expect(hashOf(px) != h0, "one changed pixel byte changes the image fingerprint");
    }
  }

  // 12. timeSec is invisible to reuse unless the material animates.
  {
    glim::paint::ReuseCache cache;
    glim::paint::Context a;
    auto metal = glim::paint::Lens::liquidMetal(glim::Color{0xd8a84eff});
    metal.motion.period = 0.f;
    metal.timeSec = 0.f;
    recordGlassAt(a, metal);
    const glim::paint::FramePacket first = cache.encode(a.scene(), 1.f);
    expect(first.stats.reuseMisses == 1, "metal first encode misses");
    glim::paint::Context b;
    auto metalLater = glim::paint::Lens::liquidMetal(glim::Color{0xd8a84eff});
    metalLater.motion.period = 0.f;
    metalLater.timeSec = 7.5f;
    recordGlassAt(b, metalLater);
    const glim::paint::FramePacket second = cache.encode(b.scene(), 1.f);
    expect(
        second.stats.reuseHits == 1 && second.stats.reuseMisses == 0,
        "still metal time change still hits"
    );
    // Flowing metal (the preset) re-encodes as time advances.
    glim::paint::Context c;
    recordGlassAt(c, glim::paint::Lens::liquidMetal());
    cache.encode(c.scene(), 1.f);
    glim::paint::Context d;
    auto flowing = glim::paint::Lens::liquidMetal();
    flowing.timeSec = 1.f;
    recordGlassAt(d, flowing);
    expect(cache.encode(d.scene(), 1.f).stats.reuseMisses == 1, "flowing metal time misses");
    // Between surges the surface rests: those frames reuse the raster.
    glim::paint::Context e;
    auto resting = glim::paint::Lens::liquidMetal();
    resting.motion =
        glim::paint::LensMotion{4.5f, 0.6f, 0.55f, glim::paint::LensMotionCurve::Spring, 0.2f};
    resting.timeSec = 3.f;
    recordGlassAt(e, resting);
    cache.encode(e.scene(), 1.f);
    glim::paint::Context f;
    resting.timeSec = 4.f;
    recordGlassAt(f, resting);
    const glim::paint::FramePacket rest = cache.encode(f.scene(), 1.f);
    expect(rest.stats.reuseHits == 1 && rest.stats.reuseMisses == 0, "resting metal hits");
  }

  // 13. Rippling water time dirties the isolate.
  {
    glim::paint::ReuseCache cache;
    glim::paint::Context a;
    auto water = glim::paint::Lens::water();
    water.rippleAmp = 1.5f;
    water.timeSec = 0.f;
    recordGlassAt(a, water);
    cache.encode(a.scene(), 1.f);
    glim::paint::Context b;
    auto waterLater = glim::paint::Lens::water();
    waterLater.rippleAmp = 1.5f;
    waterLater.timeSec = 7.5f;
    recordGlassAt(b, waterLater);
    const glim::paint::FramePacket second = cache.encode(b.scene(), 1.f);
    expect(
        second.stats.reuseMisses == 1 && second.stats.reuseHits == 0,
        "rippling water time change misses"
    );
  }

  // 14. Every material override is part of the structure: changing one
  // re-encodes the plate instead of reusing the stale look.
  {
    using glim::paint::Lens;
    const auto missesAfter = [](void (*tweak)(Lens&)) {
      glim::paint::ReuseCache cache;
      glim::paint::Context a;
      recordGlassAt(a, Lens::liquidMetal());
      cache.encode(a.scene(), 1.f);
      glim::paint::Context b;
      Lens tweaked = Lens::liquidMetal();
      tweak(tweaked);
      recordGlassAt(b, tweaked);
      return cache.encode(b.scene(), 1.f).stats.reuseMisses == 1;
    };
    expect(missesAfter([](Lens& g) { g.swellAmp = 0.3f; }), "swellAmp change misses");
    expect(missesAfter([](Lens& g) { g.swellScale.x = 1.f; }), "swellScale.x change misses");
    expect(missesAfter([](Lens& g) { g.swellScale.y = 2.f; }), "swellScale.y change misses");
    expect(missesAfter([](Lens& g) { g.envLight = 0.5f; }), "envLight change misses");
    const auto missesAt = [](void (*tweak)(Lens&)) {
      glim::paint::ReuseCache cache;
      glim::paint::Context a;
      Lens base = Lens::liquidMetal();
      base.timeSec = 1.f;
      recordGlassAt(a, base);
      cache.encode(a.scene(), 1.f);
      glim::paint::Context b;
      tweak(base);
      recordGlassAt(b, base);
      return cache.encode(b.scene(), 1.f).stats.reuseMisses == 1;
    };
    expect(missesAt([](Lens& g) { g.motion.travel = 0.3f; }), "motion.travel change misses");
    expect(missesAt([](Lens& g) { g.motion.period = 6.f; }), "motion.period change misses");
    expect(
        missesAt([](Lens& g) { g.motion.curve = glim::paint::LensMotionCurve::Smooth; }),
        "motion.curve change misses"
    );
    expect(missesAfter([](Lens& g) { g.envSoft = 0.2f; }), "envSoft change misses");
    expect(missesAfter([](Lens& g) { g.absorbK = 0.5f; }), "absorbK change misses");
  }

  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return EXIT_FAILURE;
  }
  std::cout << "reuse_test ok\n";
  return EXIT_SUCCESS;
}
