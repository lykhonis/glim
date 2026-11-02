#pragma once

#include <glim/math.h>
#include <glim/paint/Scene.h>

#include <cstdint>

namespace glim::paint {

class Context;

class Overlay {
 public:
  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }
  void toggleExpanded() { expanded_ = !expanded_; }
  bool expanded() const { return expanded_; }

  void tick(float dtSeconds);
  void setStats(const Stats& stats) { statsPending_ = stats; }
  // Window-space rects the collapsed chip must not cover (native
  // seats always composite above the scene). Copied; capped.
  void setAvoid(const Rect* rects, int count);
  // True when the displayed snapshot changed since the last record
  // (initially true; interval promotion in tick sets it). The harness
  // Frame-tests this alongside its dirty query so the HUD refreshes at
  // the snapshot rate with no per-example timer. False while disabled
  // (a disabled overlay records nothing, so it is never dirty).
  bool statsDirty() const { return enabled_ && statsChanged_; }
  void record(Context& context, Rect safeArea);

 private:
  void drawText(Context& context, Vec2 origin, const char* text, float sizePx, Color color);

  bool enabled_ = false;
  bool expanded_ = false;
  bool started_ = false;
  float fps_ = 0.f;
  float windowTime_ = 0.f;
  unsigned windowFrames_ = 0;
  float lastFrameMs_ = 0.f;
  Stats statsPending_{};
  Stats statsDisplayed_{};
  float statsAge_ = 0.f;
  bool statsChanged_ = true;
  static constexpr float kStatsInterval = 0.5f;
  static constexpr int kSpark = 60;
  static constexpr int kMaxAvoid = 8;
  Rect avoid_[kMaxAvoid]{};
  int avoidCount_ = 0;
  float spark_[kSpark]{};
  int sparkCount_ = 0;
  int sparkHead_ = 0;
};

}  // namespace glim::paint
