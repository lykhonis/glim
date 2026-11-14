#pragma once

// AreaChart: a filled curve over evenly spaced 0..1 values, axis
// labels spread along the bottom, and a callout at the right edge.
// The newest point sits on the right edge; `shift` pushes the curve
// right by that many points (a new point sliding in), and `slots`
// fixes the spacing so a short series does not stretch. Display only.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "Gel.h"
#include "HomeTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct AreaChart : WithSemantic {
  static constexpr const char* kKind = "AreaChart";
  static constexpr float kH = 180.0f;
  static constexpr float kTop = 44.0f;   // room for the callout
  static constexpr float kAxis = 24.0f;  // label strip
  static constexpr float kColumn = 4.0f;
  // Points kept past the left edge at rest, so a shift up to this
  // many points never uncovers the left side.
  static constexpr int kLead = 2;
  std::vector<float> values;  // 0..1, oldest first
  std::size_t slots = 0;  // points per full series; 0 = values.size()
  float shift = 0.0f;     // 0..kLead points
  std::vector<std::string> labels;
  std::string callout;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(
      isUnbounded(c.maxW) ? 480.0f : c.maxW, kH, c
    );
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, callout);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const std::uint32_t ink = themeColor(e, kThemeInkOnLight);
    const std::uint32_t top =
      themeColor(e, kShowAccentTop, Show::kAccentTop);
    const std::uint32_t bottom =
      themeColor(e, kShowAccentBottom, Show::kAccentBottom);
    const float base = box.y + box.h - kAxis;
    const float span = base - (box.y + kTop);
    const float size = themeFloat(e, kThemeTypeLine, Tokens::kTypeLine);

    for (std::size_t i = 0; i < labels.size(); ++i) {
      const float slot = box.w / static_cast<float>(labels.size());
      const float lw =
        static_cast<float>(labels[i].size()) * Tokens::kTypeAdvance;
      sink.text(
        box.x + slot * (static_cast<float>(i) + 0.5f) - lw * 0.5f,
        base + 6.0f,
        labels[i],
        withAlpha(ink, 0.55f),
        size
      );
    }
    if (values.size() < 2) return;

    const std::size_t n = values.size();
    const float visible = static_cast<float>(
      std::max<std::size_t>(std::max(slots, n), kLead + 2) - 1 - kLead
    );
    const float dx = box.w / visible;
    const float right = box.x + box.w;
    const float x0 = right - (static_cast<float>(n - 1) -
                              std::clamp(shift, 0.0f, float{kLead})) *
                               dx;
    auto valueAt = [&](float x) {
      const float f = std::max(0.0f, (x - x0) / dx);
      const std::size_t i =
        std::min(static_cast<std::size_t>(f), n - 2);
      const float t = std::min(f - static_cast<float>(i), 1.0f);
      return values[i] + (values[i + 1] - values[i]) * t;
    };
    auto yAt = [&](float v) {
      return base - std::clamp(v, 0.0f, 1.0f) * span;
    };

    // Columns sit on a fixed pixel grid; only their heights scroll.
    for (float x = std::max(
           box.x, box.x + std::floor((x0 - box.x) / kColumn) * kColumn
         );
         x < right;
         x += kColumn) {
      const float y = yAt(valueAt(x + kColumn * 0.5f));
      sink.fillGradient(
        {x, y, std::min(kColumn, right - x), base - y},
        0,
        withAlpha(top, 0.55f),
        withAlpha(bottom, 0.04f)
      );
    }
    std::vector<float> line;
    for (std::size_t i = 0; i < n; ++i) {
      line.push_back(x0 + dx * static_cast<float>(i));
      line.push_back(yAt(values[i]));
    }
    sink.pushClip(box);
    sink.polyline(line.data(), n, 2.0f, 0.0f, top);
    sink.pop();

    const float px = right;
    const float py = yAt(valueAt(right - 4.0f));
    sink.arc(
      px - 4.0f,
      py,
      3.0f,
      6.0f,
      0.0f,
      360.0f,
      themeColor(e, kShowCardTop, Show::kCardTop)
    );
    sink.arc(px - 4.0f, py, 5.5f, 2.0f, 0.0f, 360.0f, top);
    if (callout.empty()) return;
    const float cw =
      static_cast<float>(callout.size()) * Tokens::kTypeAdvance + 24.0f;
    const Box pill{
      std::min(px - cw * 0.5f, box.x + box.w - cw),
      std::max(box.y, py - 48.0f),
      cw,
      30.0f
    };
    sink.fillRounded(
      pill, 15, themeColor(e, kShowThumbTop, Show::kThumbTop)
    );
    sink.stroke(
      pill, 15, 1.0f, themeColor(e, kShowCardStroke, Show::kCardStroke)
    );
    sink.text(
      pill.x + 12.0f, pill.y + 8.0f, callout, ShowLight::kInk, size
    );
  }
};

inline Block areaChart(AreaChart c) { return finish(std::move(c)); }

}  // namespace toolkit
}  // namespace glim
