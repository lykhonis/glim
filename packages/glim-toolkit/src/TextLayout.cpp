#include <string>
#include <vector>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/Toolkit.h"

namespace glim {
namespace toolkit {
namespace detail {

float clampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

Box measureText(const std::string& text, Constraints c, const Engine& e) {
  float w = static_cast<float>(text.size()) * Tokens::kTypeAdvance;
  if (const Measurer* m = e.get<Measurer>()) {
    if (m->measure) w = m->measure(text.c_str(), Tokens::kTypeLine);
  }
  return resolveSize(w, Tokens::kTypeLine, c);
}

std::vector<std::string> wrapText(const std::string& text, float maxW) {
  if (isUnbounded(maxW)) return {text};
  float adv = Tokens::kTypeAdvance;
  int per = static_cast<int>(maxW / adv);
  if (per < 1) per = 1;
  // Split on spaces (collapse runs); empty text is one empty line.
  std::vector<std::string> words;
  std::string cur;
  for (char ch : text) {
    if (ch == ' ') {
      if (!cur.empty()) {
        words.push_back(cur);
        cur.clear();
      }
    } else {
      cur.push_back(ch);
    }
  }
  if (!cur.empty()) words.push_back(cur);
  if (words.empty()) return {""};
  std::vector<std::string> lines;
  std::string line;
  float lineW = 0;
  auto flushWord = [&](const std::string& w) {
    float wW = static_cast<float>(w.size()) * adv;
    if (line.empty()) {
      line = w;
      lineW = wW;
    } else if (lineW + adv + wW <= maxW + 0.001f) {
      line += ' ';
      line += w;
      lineW += adv + wW;
    } else {
      lines.push_back(line);
      line = w;
      lineW = wW;
    }
  };
  for (const auto& w : words) {
    if (static_cast<float>(w.size()) * adv <= maxW + 0.001f) {
      flushWord(w);
    } else {
      // Long word: chunk at char boundaries.
      std::size_t k = 0;
      while (k < w.size()) {
        std::size_t n = static_cast<std::size_t>(per);
        if (n > w.size() - k) n = w.size() - k;
        flushWord(w.substr(k, n));
        k += n;
      }
    }
  }
  if (!line.empty()) lines.push_back(line);
  if (lines.empty()) lines.push_back("");
  return lines;
}

std::vector<std::string> truncateLines(std::vector<std::string> lines, float maxW, float maxH) {
  float lineH = Tokens::kTypeLine;
  float adv = Tokens::kTypeAdvance;
  int maxLines = static_cast<int>(maxH / lineH);
  if (maxLines < 1) maxLines = 1;
  if (static_cast<int>(lines.size()) <= maxLines) return lines;
  lines.resize(static_cast<std::size_t>(maxLines));
  std::string& last = lines.back();
  const std::string dots = "...";
  float dotsW = static_cast<float>(dots.size()) * adv;
  float lastW = static_cast<float>(last.size()) * adv;
  if (lastW + dotsW <= maxW + 0.001f) {
    last += dots;
    return lines;
  }
  int per = isUnbounded(maxW) ? static_cast<int>(last.size()) : static_cast<int>(maxW / adv);
  if (per < 0) per = 0;
  int keep = per - static_cast<int>(dots.size());
  if (keep < 0) {
    // Narrower than "...": as many dots as fit.
    int n = per < 0 ? 0 : per;
    last = dots.substr(0, static_cast<std::size_t>(n));
    return lines;
  }
  if (keep > static_cast<int>(last.size())) keep = static_cast<int>(last.size());
  last = last.substr(0, static_cast<std::size_t>(keep)) + dots;
  return lines;
}

std::vector<std::string> layoutLines(const std::string& text, float maxW, float maxH, bool wrap) {
  std::vector<std::string> lines = wrap ? wrapText(text, maxW) : std::vector<std::string>{text};
  if (isUnbounded(maxH)) return lines;
  return truncateLines(std::move(lines), maxW, maxH);
}

Box resolveSize(float naturalW, float naturalH, Constraints c) {
  float w = (c.minW == c.maxW) ? c.maxW : (naturalW < c.minW ? c.minW : naturalW);
  float h = (c.minH == c.maxH) ? c.maxH : (naturalH < c.minH ? c.minH : naturalH);
  return {0, 0, w, h};
}

Constraints inset(Constraints c, float dx, float dy) {
  if (!isUnbounded(c.maxW)) {
    c.maxW -= dx;
    if (c.maxW < 0) c.maxW = 0;
  }
  if (!isUnbounded(c.maxH)) {
    c.maxH -= dy;
    if (c.maxH < 0) c.maxH = 0;
  }
  if (c.minW > 0) {
    c.minW -= dx;
    if (c.minW < 0) c.minW = 0;
  }
  if (c.minH > 0) {
    c.minH -= dy;
    if (c.minH < 0) c.minH = 0;
  }
  if (c.minW > c.maxW) c.minW = c.maxW;
  if (c.minH > c.maxH) c.minH = c.maxH;
  return c;
}

}  // namespace detail
}  // namespace toolkit
}  // namespace glim
