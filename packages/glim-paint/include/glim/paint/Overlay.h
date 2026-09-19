#pragma once

#include <cstdint>

#include <glim/math.h>
#include <glim/paint/Scene.h>

namespace glim::paint {

class Context;

class Overlay {
public:
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool enabled() const { return enabled_; }
    void toggleExpanded() { expanded_ = !expanded_; }
    bool expanded() const { return expanded_; }

    void tick(float dtSeconds);
    void setStats(const Stats& stats) { stats_ = stats; }
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
    Stats stats_{};
    static constexpr int kSpark = 60;
    float spark_[kSpark]{};
    int sparkCount_ = 0;
    int sparkHead_ = 0;
};

}  // namespace glim::paint
