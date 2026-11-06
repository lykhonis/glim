#pragma once

// Telemetry panels: ordinary labels/charts fed by pull (TelemetryHub).
// Null hub renders deterministic placeholders so golden snapshots stay
// stable with no live loop running.

#include <cstdio>
#include <string>
#include <vector>

#include "Chart.h"
#include "glim/toolkit.h"
#include "glim/toolkit/host/Telemetry.h"

namespace glim {
namespace toolkit {

inline float hubFps(const host::FrameTelemetry& last) {
  if (last.frameMs <= 0.f) return 0.f;
  return 1000.0f / last.frameMs;
}

// 0..1-normalized frame times for the sparkline; 16.7ms maps to ~0.6.
inline std::vector<float> frameSparkValues(const host::TelemetryHub* hub
) {
  std::vector<float> out;
  if (hub == nullptr) {
    out = {0.3f, 0.5f, 0.4f, 0.6f, 0.5f, 0.7f, 0.4f, 0.5f};
    return out;
  }
  auto hist = hub->history();
  out.reserve(hist.size() > 0 ? hist.size() : 0);
  for (const auto& s : hist) {
    float v = s.frameMs / 33.0f;
    if (v < 0.f) v = 0.f;
    if (v > 1.f) v = 1.f;
    out.push_back(v);
  }
  if (out.empty()) out = {0.3f, 0.5f, 0.4f, 0.6f};
  return out;
}

inline Block telemetrySummary(const host::TelemetryHub* hub) {
  host::FrameTelemetry s{};
  bool live = false;
  if (hub != nullptr && !hub->empty()) {
    s = hub->last();
    live = true;
  }
  char fps[64], frame[64], rec[64], enc[64];
  std::snprintf(
      fps, sizeof(fps), live ? "FPS %.0f" : "FPS --", hubFps(s)
  );
  std::snprintf(
      frame,
      sizeof(frame),
      live ? "frame %.1f ms" : "frame -- ms",
      static_cast<double>(s.frameMs)
  );
  std::snprintf(
      rec,
      sizeof(rec),
      live ? "record %.1f ms" : "record -- ms",
      static_cast<double>(s.recordMs)
  );
  std::snprintf(
      enc,
      sizeof(enc),
      live ? "encode %.1f ms" : "encode -- ms",
      static_cast<double>(s.encodeMs)
  );
  return column(
      gap(4),
      lines(live ? "// LIVE" : "// IDLE"),
      lines(fps),
      lines(frame),
      lines(rec),
      lines(enc)
  );
}

inline Block telemetryDetail(const host::TelemetryHub* hub) {
  host::FrameTelemetry s{};
  bool live = false;
  if (hub != nullptr && !hub->empty()) {
    s = hub->last();
    live = true;
  }
  char draws[64], inst[64], iso[64], tiles[64], reuse[64], size[64];
  if (live) {
    std::snprintf(draws, sizeof(draws), "draws %u", s.stats.draws);
    std::snprintf(
        inst, sizeof(inst), "instances %u", s.stats.instances
    );
    std::snprintf(
        iso,
        sizeof(iso),
        "isolates %u merged %u",
        s.stats.isolateCount,
        s.stats.mergedGroupCount
    );
    std::snprintf(
        tiles,
        sizeof(tiles),
        "tiles %u dirty %u",
        s.stats.tileCount,
        s.stats.dirtyTiles
    );
    std::snprintf(
        reuse,
        sizeof(reuse),
        "reuse %u miss %u",
        s.stats.reuseHits,
        s.stats.reuseMisses
    );
    std::snprintf(
        size,
        sizeof(size),
        "size %.0f x %.0f rev %llu",
        static_cast<double>(s.size.x),
        static_cast<double>(s.size.y),
        static_cast<unsigned long long>(s.revision)
    );
  } else {
    std::snprintf(draws, sizeof(draws), "draws --");
    std::snprintf(inst, sizeof(inst), "instances --");
    std::snprintf(iso, sizeof(iso), "isolates -- merged --");
    std::snprintf(tiles, sizeof(tiles), "tiles -- dirty --");
    std::snprintf(reuse, sizeof(reuse), "reuse -- miss --");
    std::snprintf(size, sizeof(size), "size -- rev --");
  }
  return column(
      gap(4),
      lines("// RENDERER"),
      lines(draws),
      lines(inst),
      lines(iso),
      lines(tiles),
      lines(reuse),
      lines(size)
  );
}

}  // namespace toolkit
}  // namespace glim
