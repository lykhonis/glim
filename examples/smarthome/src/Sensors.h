#pragma once

// Home sensors, pretended from frame telemetry. The app has no real
// house, so the render loop stands in for one: CPU work per frame is
// the power draw (and the heat it gives off), the frame rate is the
// sun on the roof, and frame-pacing jitter is radio noise.
//
// Frames fold into fixed half-second buckets of wall time; readings
// only use closed buckets, so numbers and the chart move twice a
// second instead of shimmering at the frame rate.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "Home.h"

namespace glim {
namespace home {

struct FrameSample {
  float time = 0.0f;      // seconds since start
  float frameMs = 16.7f;  // interval between painted frames
  float workMs = 0.0f;    // build + record + encode
};

inline constexpr float kBucketSec = 0.5f;
inline constexpr std::size_t kLogBuckets = 48;    // 24 s of history
inline constexpr std::size_t kRecentBuckets = 2;  // the last second
// Work per frame that reads as a fully loaded home.
inline constexpr float kEnergyFullMs = 6.0f;
// Input paints land between vsyncs; an interval this short is the
// front half of a split frame, not a frame of its own.
inline constexpr float kSplitFrameMs = 4.0f;

// Running sums over the frames in one bucket.
struct Bucket {
  long index = 0;
  int frames = 0;
  float frameMs = 0.0f;
  float frameMsSq = 0.0f;
  float workMs = 0.0f;
};

struct SensorLog {
  std::vector<Bucket> closed;  // oldest first, at most kLogBuckets
  Bucket open;
  // Split-frame interval and work waiting for the frame that ends it.
  float carryMs = 0.0f;
  float carryWorkMs = 0.0f;
};

inline SensorLog logSample(SensorLog log, FrameSample s) {
  if (s.frameMs < kSplitFrameMs) {
    log.carryMs += s.frameMs;
    log.carryWorkMs += s.workMs;
    return log;
  }
  s.frameMs += log.carryMs;
  s.workMs += log.carryWorkMs;
  log.carryMs = log.carryWorkMs = 0.0f;
  const long index = static_cast<long>(std::floor(s.time / kBucketSec));
  if (log.open.frames > 0 && index != log.open.index) {
    log.closed.push_back(log.open);
    if (log.closed.size() > kLogBuckets)
      log.closed.erase(log.closed.begin());
    log.open = {};
  }
  log.open.index = index;
  log.open.frames += 1;
  log.open.frameMs += s.frameMs;
  log.open.frameMsSq += s.frameMs * s.frameMs;
  log.open.workMs += s.workMs;
  return log;
}

struct Readings {
  float energy = 0.0f;  // 0..1
  float indoorC = 0.0f;
  float fps = 0.0f;
  float solarKwh = 0.0f;
  std::vector<float>
      solarCurve;       // 0..1 per closed bucket, oldest first
  long curveHead = -1;  // bucket index of the newest point
  int wifiBars = 0;     // 1..4
  std::string wifiQuality;
  float latencyMs = 0.0f;
  float jitterMs = 0.0f;
};

namespace detail {

inline Bucket merged(
    std::vector<Bucket>::const_iterator a,
    std::vector<Bucket>::const_iterator b
) {
  Bucket m;
  for (auto it = a; it != b; ++it) {
    m.frames += it->frames;
    m.frameMs += it->frameMs;
    m.frameMsSq += it->frameMsSq;
    m.workMs += it->workMs;
  }
  return m;
}

inline float meanFrameMs(const Bucket& b) {
  return b.frames ? b.frameMs / static_cast<float>(b.frames) : 0.0f;
}

inline float meanWorkMs(const Bucket& b) {
  return b.frames ? b.workMs / static_cast<float>(b.frames) : 0.0f;
}

inline float fpsOf(const Bucket& b) {
  const float ms = meanFrameMs(b);
  return ms > 0.0f ? 1000.0f / ms : 0.0f;
}

inline float jitterOf(const Bucket& b) {
  if (b.frames == 0) return 0.0f;
  const float mean = meanFrameMs(b);
  return std::sqrt(std::max(
      0.0f, b.frameMsSq / static_cast<float>(b.frames) - mean * mean
  ));
}

}  // namespace detail

inline Readings readings(const SensorLog& log) {
  Readings r;
  if (log.closed.empty()) return r;
  const auto end = log.closed.end();
  const Bucket recent = detail::merged(
      end - static_cast<std::ptrdiff_t>(
                std::min(kRecentBuckets, log.closed.size())
            ),
      end
  );
  const Bucket all = detail::merged(log.closed.begin(), end);
  r.energy = std::clamp(
      detail::meanWorkMs(recent) / kEnergyFullMs, 0.0f, 1.0f
  );
  r.indoorC =
      std::clamp(18.0f + 1.5f * detail::meanWorkMs(all), 16.0f, 30.0f);
  r.fps = detail::fpsOf(recent);
  // One kWh per 5 fps: a steady 60 Hz day collects 12 kWh.
  r.solarKwh = r.fps / 5.0f;
  r.latencyMs = detail::meanFrameMs(recent);
  r.jitterMs = detail::jitterOf(recent);

  // Curve against the window's peak rate: half the peak or less sits
  // at the floor, so a vsync-locked loop reads high at any refresh.
  float peak = 0.0f;
  for (const Bucket& b : log.closed)
    peak = std::max(peak, detail::fpsOf(b));
  for (const Bucket& b : log.closed) {
    const float t =
        peak > 0.0f ? (detail::fpsOf(b) / peak - 0.5f) / 0.5f : 0.0f;
    r.solarCurve.push_back(0.2f + 0.7f * std::clamp(t, 0.0f, 1.0f));
  }
  r.curveHead = log.closed.back().index;

  if (r.jitterMs < 1.0f) {
    r.wifiBars = 4, r.wifiQuality = "Excellent";
  } else if (r.jitterMs < 2.5f) {
    r.wifiBars = 3, r.wifiQuality = "Good";
  } else if (r.jitterMs < 5.0f) {
    r.wifiBars = 2, r.wifiQuality = "Fair";
  } else {
    r.wifiBars = 1, r.wifiQuality = "Weak";
  }
  return r;
}

// Headless runs and tests have no host loop; this stands in for 24 s
// of one, drifting slowly and fully deterministic.
inline const SensorLog& demoLog() {
  static const SensorLog kDemo = [] {
    SensorLog log;
    float t = 0.0f;
    for (int i = 0;
         t < kBucketSec * static_cast<float>(kLogBuckets + 1);
         ++i) {
      const float ms = 17.5f + 3.0f * std::sin(t * 0.55f) +
                       1.6f * std::sin(t * 1.7f) +
                       (i % 2 ? 0.8f : -0.8f);
      log = logSample(
          std::move(log), {t, ms, 2.4f + 0.9f * std::sin(t * 0.3f)}
      );
      t += ms / 1000.0f;
    }
    return log;
  }();
  return kDemo;
}

inline std::string band(float level) {
  if (level < 0.34f) return "Low";
  if (level < 0.67f) return "Medium";
  return "High";
}

inline std::string kwhText(float kwh) {
  char out[16];
  std::snprintf(out, sizeof(out), "%.1f", static_cast<double>(kwh));
  return out;
}

inline std::string msText(float ms) {
  char out[16];
  std::snprintf(out, sizeof(out), "%.1f ms", static_cast<double>(ms));
  return out;
}

inline std::string feedStatus(bool live) {
  return live ? "All devices online" : "Updates paused";
}

inline int signalBars(const Readings& r, bool wifiOn) {
  return wifiOn ? r.wifiBars : 0;
}

inline std::string latencyText(const Readings& r, bool wifiOn) {
  return wifiOn ? msText(r.latencyMs) : "-";
}

inline std::string jitterText(const Readings& r, bool wifiOn) {
  return wifiOn ? msText(r.jitterMs) : "-";
}

inline std::string wifiStatus(const Readings& r, bool wifiOn) {
  if (!wifiOn) return "Disconnected";
  return r.wifiQuality + " signal";
}

// Mean level of the lights that are on, 0 when all are off.
inline float brightness(const std::vector<Device>& devices) {
  float sum = 0.0f;
  int n = 0;
  for (const Device& d : devices)
    if (d.kind == Kind::Light && d.on) sum += d.level, ++n;
  return n == 0 ? 0.0f : sum / static_cast<float>(n);
}

// How the thermostat is doing against the indoor temperature.
inline std::string climateText(
    float indoorC, const Device& thermostat
) {
  if (!thermostat.on) return "Thermostat off";
  const float target = celsius(thermostat.level);
  const float diff = std::fabs(target - indoorC);
  if (diff < 0.3f) return "Holding at " + temperatureText(target);
  const int minutes = static_cast<int>(std::ceil(diff * 4.0f));
  const char* verb = target > indoorC ? "Heating to " : "Cooling to ";
  return verb + temperatureText(target) + ", " +
         std::to_string(minutes) + " min";
}

inline std::string energyText(float energy) {
  return band(energy) + " use";
}

inline std::string lightText(const Device& light) {
  return light.on ? "Brightness" : "Lights off";
}

// One line on how the home is doing, for the greeting.
inline std::string homeSummary(const Readings& r, float lightLevel) {
  return "Inside " + temperatureText(r.indoorC) + ", lights at " +
         percentText(lightLevel) + ", solar " + kwhText(r.solarKwh) +
         " kWh today.";
}

}  // namespace home
}  // namespace glim
