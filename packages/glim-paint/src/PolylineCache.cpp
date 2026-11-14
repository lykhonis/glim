#include <algorithm>
#include <cstring>
#include <unordered_map>

#if !defined(__wasi__) || defined(__wasm_atomics__)
#include <mutex>
#endif

#include "Strip.h"

namespace glim::paint {
namespace {

// Flattened coverage depends only on world points, width and pitch, so a
// static polyline (map streets, routes) flattens once, not every frame.
constexpr std::size_t kMaxEntries = 512;

struct Key {
  std::vector<Vec2> points;
  float width;
  float pitch;
  bool operator==(const Key& o) const {
    return width == o.width && pitch == o.pitch && points.size() == o.points.size() &&
           std::memcmp(points.data(), o.points.data(), points.size() * sizeof(Vec2)) == 0;
  }
};

struct KeyHash {
  std::size_t operator()(const Key& k) const {
    std::uint64_t h = 1469598103934665603ull;
    const auto mix = [&](const void* p, std::size_t n) {
      const auto* b = static_cast<const unsigned char*>(p);
      for (std::size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
    };
    mix(&k.width, sizeof k.width);
    mix(&k.pitch, sizeof k.pitch);
    mix(k.points.data(), k.points.size() * sizeof(Vec2));
    return static_cast<std::size_t>(h);
  }
};

#if (defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)) || \
  (defined(__wasi__) && !defined(__wasm_atomics__))
struct Lock {};
#else
std::mutex cacheMutex;
struct Lock {
  std::lock_guard<std::mutex> guard{cacheMutex};
};
#endif
std::unordered_map<Key, std::vector<Strip>, KeyHash> cache;

}  // namespace

void flattenPolyline(const Polyline& pl, std::vector<Strip>& out) {
  if (pl.points.size() < 2) return;
  Key key{pl.points, pl.width, pl.dotPitch};
  {
    [[maybe_unused]] const Lock lock;
    if (const auto it = cache.find(key); it != cache.end()) {
      out.insert(out.end(), it->second.begin(), it->second.end());
      return;
    }
  }
  std::vector<Strip> strips;
  if (pl.dotPitch > 0.f) {
    flattenDottedPolyline(pl.points.data(), pl.points.size(), pl.width, pl.dotPitch, strips);
  } else {
    flattenSolidPolyline(pl.points.data(), pl.points.size(), pl.width, strips);
  }
  out.insert(out.end(), strips.begin(), strips.end());
  [[maybe_unused]] const Lock lock;
  if (cache.size() >= kMaxEntries) cache.clear();
  cache.emplace(std::move(key), std::move(strips));
}

}  // namespace glim::paint
