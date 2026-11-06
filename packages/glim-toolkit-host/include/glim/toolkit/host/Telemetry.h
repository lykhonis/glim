#pragma once

// Frame telemetry: push + pull seam for all layers.
//
// The host loop is the only writer. It samples shell timing (frameMs),
// toolkit record timing (buildMs/recordMs), and paint/gpu stats
// (encodeMs + paint::Stats) into one snapshot per painted frame, then
// fans out both ways:
//   - pull: TelemetryHub ring buffer, read from Build closures to render
//     dashboard labels/charts in user space;
//   - push: TelemetrySink callback, for logging or delivering elsewhere.
// Null hub/sink (default) skips everything — zero cost when unused,
// same contract as A11ySink.

#include <glim/math.h>
#include <glim/paint/Scene.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace glim {
namespace toolkit {
namespace host {

struct FrameTelemetry {
  float frameMs = 0.f;
  float buildMs = 0.f;
  float recordMs = 0.f;
  float encodeMs = 0.f;
  paint::Stats stats{};
  Vec2 size{};
  Rect safeArea{};
  float time = 0.f;
  std::uint64_t revision = 0;
};

using TelemetrySink = std::function<void(const FrameTelemetry&)>;

class TelemetryHub {
 public:
  explicit TelemetryHub(std::size_t capacity = 240) : capacity_(capacity == 0 ? 1 : capacity) {}

  void push(const FrameTelemetry& sample) {
    std::lock_guard<std::mutex> lock(mutex_);
    last_ = sample;
    hasLast_ = true;
    if (ring_.size() < capacity_) {
      ring_.push_back(sample);
    } else {
      ring_[head_] = sample;
      head_ = (head_ + 1) % ring_.size();
    }
    ++pushes_;
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !hasLast_;
  }

  FrameTelemetry last() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_;
  }

  // Oldest-first snapshot, capped at capacity. Copy under lock; callers
  // own the result and can render charts without holding anything.
  std::vector<FrameTelemetry> history() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<FrameTelemetry> out;
    out.reserve(ring_.size());
    if (ring_.size() < capacity_ || pushes_ <= ring_.size()) {
      out = ring_;
    } else {
      for (std::size_t i = 0; i < ring_.size(); ++i) {
        out.push_back(ring_[(head_ + i) % ring_.size()]);
      }
    }
    return out;
  }

  std::size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ring_.size();
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    ring_.clear();
    head_ = 0;
    pushes_ = 0;
    hasLast_ = false;
    last_ = FrameTelemetry{};
  }

 private:
  mutable std::mutex mutex_;
  std::size_t capacity_ = 240;
  std::vector<FrameTelemetry> ring_;
  std::size_t head_ = 0;
  std::size_t pushes_ = 0;
  FrameTelemetry last_{};
  bool hasLast_ = false;
};

}  // namespace host
}  // namespace toolkit
}  // namespace glim
