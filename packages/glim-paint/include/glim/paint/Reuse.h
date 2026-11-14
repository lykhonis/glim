#pragma once

#include <glim/math.h>
#include <glim/paint/FramePacket.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace glim::paint {

// Structural hash identity (translation normalized); foreign content is volatile.
// Single-threaded: one ReuseCache per encode stream.
class ReuseCache {
 public:
  ReuseCache();
  ~ReuseCache();
  ReuseCache(const ReuseCache&) = delete;
  ReuseCache& operator=(const ReuseCache&) = delete;

  // Hit: cached quads reused with rewritten origins; miss: full merge+encode.
  FramePacket encode(const Scene& scene, float pixelRatio = 1.0f);

  void clear();
  std::size_t size() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Memoized per-image fingerprints; slots are append-only, validated against the live slot.
struct SceneHashCache {
  struct Entry {
    const StoredImage* ptr = nullptr;
    int w = 0;
    int h = 0;
    std::size_t bytes = 0;
    std::uint64_t fp = 0;
  };
  std::unordered_map<std::uint32_t, Entry> images;
};

// Translation-normalized structural hash; out params optional.
std::uint64_t hashSceneStructure(
  const Scene& scene,
  float pixelRatio,
  Vec2* outOrigin = nullptr,
  bool* outHasForeign = nullptr,
  bool* outHas3D = nullptr
);
std::uint64_t hashSceneStructure(
  const Scene& scene,
  float pixelRatio,
  SceneHashCache& cache,
  Vec2* outOrigin = nullptr,
  bool* outHasForeign = nullptr,
  bool* outHas3D = nullptr
);

FramePacket encodeWithReuse(const Scene& scene, ReuseCache& cache, float pixelRatio = 1.0f);

}  // namespace glim::paint
