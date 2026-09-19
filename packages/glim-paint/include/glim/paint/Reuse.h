#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include <glim/math.h>
#include <glim/paint/FramePacket.h>

namespace glim::paint {

// Raster reuse (Renderer-internal policy, compositor.md).
//
// A Scene is immediate-mode (rebuilt every beginFrame/finish), so identity is
// a structural hash of the unmerged tree, not a pointer. Translation is
// normalized out of the key: a scene that only moved reuses the cached
// encode with rewritten origins. Scale, clip, radius, color, or glyph-UV
// changes miss and rebuild.
//
// Rules:
// - No app-facing cache API. No RepaintBoundary, no GroupParams hint. The
//   caller owns a ReuseCache; the compositor decides hits internally.
// - No GPU raster plates: this caches merged strips / encoded quads (CPU
//   work), never promotes a Group to an extra FrameTarget. Isolate plates
//   stay opacity / 3D / backdrop / glass only (K52).
// - Foreign Matter (imported GPU textures, e.g. video) is volatile: any
//   scene containing it always misses and is never stored.
// - Single-threaded, like the rest of v1 (K17). One ReuseCache per
//   encode stream; do not share across threads.
//
// Whole-packet granularity in this step: per-tile dirtying shares the hash
// grid and lands with the tile stage (Stats.dirtyTiles stays 0 here).
class ReuseCache {
public:
    ReuseCache();
    ~ReuseCache();
    ReuseCache(const ReuseCache&) = delete;
    ReuseCache& operator=(const ReuseCache&) = delete;

    // Encode with reuse. Miss: full merge+encode, stored, stats.reuseMisses=1.
    // Hit (exact or translation-only): cached quads reused, origins
    // rewritten on translation, stats.reuseHits=1. stats.dirtyTiles reports
    // coarse tiles (kTileSize device px) whose content changed vs the last
    // frame: 0 on an exact hit, tile count on the first frame or a resize.
    FramePacket encode(const Scene& scene, float pixelRatio = 1.0f);

    void clear();
    std::size_t size() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Structural hash of the unmerged scene with translation normalized out.
// outOrigin receives the content-bbox origin the hash is relative to.
// outHasForeign/outHas3D report volatile content / 3D transforms (both
// optional, may be nullptr).
std::uint64_t hashSceneStructure(const Scene& scene, float pixelRatio, Vec2* outOrigin = nullptr,
                                 bool* outHasForeign = nullptr, bool* outHas3D = nullptr);

FramePacket encodeWithReuse(const Scene& scene, ReuseCache& cache, float pixelRatio = 1.0f);

}  // namespace glim::paint
