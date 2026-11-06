#pragma once

// AlertCard: bracketed HUD card for the event feed. Corner brackets
// (short strokes), severity dot, two text lines. Severity is parsed
// from the log-line prefix: "! " bad, "~ " warn, else info.
#include <string>
#include <utility>
#include <vector>

#include "ShowcaseTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct AlertCard {
  static constexpr const char* kKind = "Alert";
  std::string title;
  std::string detail;
  int severity = 0;  // 0 info, 1 warn, 2 bad
  Box place(Constraints c, const Engine& e) const {
    Box t = text::measureText(title, {0, kUnbounded, 0, kUnbounded}, e);
    Box d =
        text::measureText(detail, {0, kUnbounded, 0, kUnbounded}, e);
    float w = (t.w > d.w ? t.w : d.w) + 56.0f;
    if (!isUnbounded(c.maxW)) w = c.maxW;
    return detail::resolveSize(w, 52.0f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, title);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    sink.fillGradient(
        box,
        8,
        themeColor(e, kShowCardTop, Show::kCardTop),
        themeColor(e, kShowCardBottom, Show::kCardBottom)
    );
    std::uint32_t edge =
        themeColor(e, kShowCardStroke, Show::kCardStroke);
    if (severity == 2)
      edge = themeColor(e, kShowBad, Show::kBad);
    else if (severity == 1)
      edge = themeColor(e, kShowWarn, Show::kWarn);
    // Corner brackets: 4 × L strokes, restrained 10px arms.
    const float L = 10.0f;
    auto h = [&](float x, float y, float w) {
      sink.fillRounded({x, y, w, 1.0f}, 1, edge);
    };
    auto v = [&](float x, float y, float hh) {
      sink.fillRounded({x, y, 1.0f, hh}, 1, edge);
    };
    h(box.x, box.y, L);
    v(box.x, box.y, L);
    h(box.x + box.w - L, box.y, L);
    v(box.x + box.w - 1.0f, box.y, L);
    h(box.x, box.y + box.h - 1.0f, L);
    v(box.x, box.y + box.h - L, L);
    h(box.x + box.w - L, box.y + box.h - 1.0f, L);
    v(box.x + box.w - 1.0f, box.y + box.h - L, L);
    std::uint32_t dot = themeColor(e, kShowOk, Show::kOk);
    if (severity == 2)
      dot = themeColor(e, kShowBad, Show::kBad);
    else if (severity == 1)
      dot = themeColor(e, kShowWarn, Show::kWarn);
    const float cy = box.y + box.h * 0.5f;
    sink.fillRounded({box.x + 16.0f, cy - 4.0f, 8.0f, 8.0f}, 4, dot);
    const std::uint32_t ink =
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight);
    const float size = themeFloat(e, kThemeTypeLine, Tokens::kTypeLine);
    sink.text(box.x + 30.0f, box.y + 10.0f, title, ink, size);
    sink.text(box.x + 30.0f, box.y + 10.0f + size, detail, ink, size);
  }
};

// Split "SEV title | detail" log lines into cards; newest first.
inline std::vector<AlertCard> alertCards(
    const std::vector<std::string>& log, std::size_t max = 6
) {
  std::vector<AlertCard> out;
  std::size_t n = log.size() > max ? max : log.size();
  for (std::size_t k = 0; k < n; ++k) {
    const std::string& line = log[log.size() - 1 - k];
    AlertCard a;
    std::size_t at = line.find('|');
    std::string head =
        at == std::string::npos ? line : line.substr(0, at);
    a.detail = at == std::string::npos ? "" : line.substr(at + 1);
    if (head.rfind("! ", 0) == 0) {
      a.severity = 2;
      a.title = head.substr(2);
    } else if (head.rfind("~ ", 0) == 0) {
      a.severity = 1;
      a.title = head.substr(2);
    } else {
      a.severity = 0;
      a.title = head;
    }
    while (!a.title.empty() && a.title.back() == ' ')
      a.title.pop_back();
    out.push_back(std::move(a));
  }
  return out;
}

template <class... As>
auto alertCard(AlertCard a, As&&... as) {
  configureTags(a, std::forward<As>(as)...);
  return finish(std::move(a), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
