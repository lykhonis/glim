#include "glim/toolkit/text/Text.h"

#include <string>
#include <vector>

#include "glim/toolkit/Layout.h"
#include "glim/toolkit/Measure.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {
namespace text {

Box measureText(const std::string& text, Constraints c, const Engine& e) {
  float w = static_cast<float>(text.size()) * Tokens::kTypeAdvance;
  if (auto m = e.get<Measurer>()) {
    if (m->get().measure) w = m->get().measure(text.c_str(), Tokens::kTypeLine);
  }
  return detail::resolveSize(w, Tokens::kTypeLine, c);
}

namespace {

std::vector<std::string> greedyWrap(const std::string& text, float maxW) {
  float adv = Tokens::kTypeAdvance;
  int per = static_cast<int>(maxW / adv);
  if (per < 1) per = 1;
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

}  // namespace

std::vector<std::string> wrapText(const std::string& text, float maxW, const Engine&) {
  if (isUnbounded(maxW)) return {text};
  return greedyWrap(text, maxW);
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
    int n = per < 0 ? 0 : per;
    last = dots.substr(0, static_cast<std::size_t>(n));
    return lines;
  }
  if (keep > static_cast<int>(last.size())) keep = static_cast<int>(last.size());
  last = last.substr(0, static_cast<std::size_t>(keep)) + dots;
  return lines;
}

std::vector<std::string> layoutLines(
    const std::string& text, float maxW, float maxH, bool wrap, const Engine& e
) {
  std::vector<std::string> lines = wrap ? wrapText(text, maxW, e) : std::vector<std::string>{text};
  if (isUnbounded(maxH)) return lines;
  return truncateLines(std::move(lines), maxW, maxH);
}

std::vector<PlacedLine> layoutPlacedLines(
    const std::string& text, const Box& box, bool wrap, int align, const Engine& e
) {
  std::vector<std::string> lines = layoutLines(text, box.w, box.h, wrap, e);
  std::vector<PlacedLine> out;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    float lw = static_cast<float>(lines[i].size()) * Tokens::kTypeAdvance;
    float x = box.x;
    if (align == kAlignCenter) x += (box.w - lw) * 0.5f;
    if (align == kAlignRight) x += box.w - lw;
    out.push_back(
        {{x, box.y + static_cast<float>(i) * Tokens::kTypeLine, lw, Tokens::kTypeLine}, lines[i]}
    );
  }
  return out;
}

}  // namespace text
}  // namespace toolkit
}  // namespace glim
