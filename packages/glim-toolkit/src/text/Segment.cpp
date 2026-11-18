#include "Segment.h"

namespace glim::toolkit::text {

namespace {

constexpr char32_t kReplacement = 0xFFFD;
constexpr char32_t kZwj = 0x200D;

bool continuation(char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }

bool in(char32_t c, char32_t lo, char32_t hi) { return c >= lo && c <= hi; }

std::size_t prevPoint(std::string_view s, std::size_t i) {
  if (i == 0) return 0;
  --i;
  while (i > 0 && continuation(s[i])) --i;
  return i;
}

std::size_t nextPoint(std::string_view s, std::size_t i) {
  std::size_t next = s.size();
  decodeAt(s, i, &next);
  return next;
}

// Marks that attach to the code point before them.
bool extends(char32_t c) {
  return in(c, 0x0300, 0x036F) || in(c, 0x0483, 0x0489) || in(c, 0x0591, 0x05BD) ||
         in(c, 0x0610, 0x061A) || in(c, 0x064B, 0x065F) || in(c, 0x0900, 0x0903) ||
         in(c, 0x093A, 0x094F) || in(c, 0x0951, 0x0957) || in(c, 0x0962, 0x0963) || c == 0x0E31 ||
         in(c, 0x0E34, 0x0E3A) || in(c, 0x0E47, 0x0E4E) || in(c, 0x1AB0, 0x1AFF) ||
         in(c, 0x1DC0, 0x1DFF) || c == 0x200C || c == kZwj || in(c, 0x20D0, 0x20FF) ||
         in(c, 0xFE00, 0xFE0F) || in(c, 0xFE20, 0xFE2F) || in(c, 0x1F3FB, 0x1F3FF) ||
         in(c, 0xE0020, 0xE007F) || in(c, 0xE0100, 0xE01EF);
}

bool pictographic(char32_t c) {
  return in(c, 0x1F000, 0x1FAFF) || in(c, 0x2600, 0x27BF) || in(c, 0x2300, 0x23FF) ||
         in(c, 0x2B00, 0x2BFF) || in(c, 0x2194, 0x21AA) || c == 0x00A9 || c == 0x00AE ||
         c == 0x203C || c == 0x2049 || c == 0x2122 || c == 0x2139 || c == 0x3030 || c == 0x303D ||
         c == 0x3297 || c == 0x3299;
}

bool regional(char32_t c) { return in(c, 0x1F1E6, 0x1F1FF); }

bool space(char32_t c) {
  return c == ' ' || c == '\t' || c == 0x00A0 || c == 0x1680 || in(c, 0x2000, 0x200A) ||
         c == 0x202F || c == 0x205F || c == 0x3000;
}

// Whether a cluster boundary falls between the code points at a and b.
bool breaks(std::string_view s, std::size_t a, std::size_t b) {
  const char32_t x = decodeAt(s, a);
  const char32_t y = decodeAt(s, b);
  if (x == '\r' && y == '\n') return false;
  if (x == '\n' || x == '\r' || y == '\n' || y == '\r') return true;
  if (extends(y)) return false;
  if (x == kZwj && pictographic(y)) return false;
  if (regional(x) && regional(y)) {
    // Flags pair up from the start of a run of indicators.
    std::size_t run = 0;
    for (std::size_t p = a;; p = prevPoint(s, p)) {
      if (!regional(decodeAt(s, p))) break;
      ++run;
      if (p == 0) break;
    }
    return run % 2 == 0;
  }
  return true;
}

enum class Kind { Word, Space, Line, Other };

Kind kindAt(std::string_view s, std::size_t i) {
  const char32_t c = decodeAt(s, i);
  if (c == '\n' || c == '\r') return Kind::Line;
  if (space(c)) return Kind::Space;
  return isWordPoint(c) ? Kind::Word : Kind::Other;
}

}  // namespace

char32_t decodeAt(std::string_view s, std::size_t i, std::size_t* next) {
  if (i >= s.size()) {
    if (next) *next = s.size();
    return 0;
  }
  const auto b = static_cast<unsigned char>(s[i]);
  const int extra = b < 0x80 ? 0 : (b >> 5) == 6 ? 1 : (b >> 4) == 14 ? 2 : (b >> 3) == 30 ? 3 : -1;
  if (extra <= 0 || i + extra >= s.size()) {
    if (next) *next = i + 1;
    return extra == 0 ? b : kReplacement;
  }
  char32_t c = b & (0x3F >> extra);
  for (int k = 1; k <= extra; ++k) {
    if (!continuation(s[i + k])) {
      if (next) *next = i + k;
      return kReplacement;
    }
    c = (c << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
  }
  if (next) *next = i + extra + 1;
  return c;
}

std::size_t snapPoint(std::string_view s, std::size_t i) {
  if (i >= s.size()) return s.size();
  while (i > 0 && continuation(s[i])) --i;
  return i;
}

std::size_t nextCluster(std::string_view s, std::size_t i) {
  std::size_t at = snapPoint(s, i);
  if (at >= s.size()) return s.size();
  std::size_t next = nextPoint(s, at);
  while (next < s.size() && !breaks(s, at, next)) {
    at = next;
    next = nextPoint(s, next);
  }
  return next;
}

std::size_t prevCluster(std::string_view s, std::size_t i) {
  std::size_t at = prevPoint(s, snapPoint(s, i));
  while (at > 0) {
    const std::size_t before = prevPoint(s, at);
    if (breaks(s, before, at)) break;
    at = before;
  }
  return at;
}

std::size_t clusterCount(std::string_view s) {
  std::size_t n = 0;
  for (std::size_t i = 0; i < s.size(); i = nextCluster(s, i)) ++n;
  return n;
}

bool isWordPoint(char32_t c) {
  if (c < 0x80) return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  return !space(c) && !pictographic(c) && !in(c, 0x2010, 0x205E) && !in(c, 0x3001, 0x3011) &&
         !in(c, 0xFF01, 0xFF0F) && !in(c, 0x00A1, 0x00BF) && c != kReplacement;
}

std::size_t nextWordEnd(std::string_view s, std::size_t i) {
  std::size_t at = snapPoint(s, i);
  while (at < s.size() && kindAt(s, at) != Kind::Word) at = nextCluster(s, at);
  while (at < s.size() && kindAt(s, at) == Kind::Word) at = nextCluster(s, at);
  return at;
}

std::size_t prevWordStart(std::string_view s, std::size_t i) {
  std::size_t at = snapPoint(s, i);
  while (at > 0 && kindAt(s, prevCluster(s, at)) != Kind::Word) at = prevCluster(s, at);
  while (at > 0 && kindAt(s, prevCluster(s, at)) == Kind::Word) at = prevCluster(s, at);
  return at;
}

Span runAt(std::string_view s, std::size_t i) {
  if (s.empty()) return {};
  std::size_t at = snapPoint(s, i);
  if (at >= s.size()) at = prevCluster(s, s.size());
  const Kind k = kindAt(s, at);
  Span span{at, nextCluster(s, at)};
  if (k == Kind::Line) return span;
  while (span.start > 0 && kindAt(s, prevCluster(s, span.start)) == k)
    span.start = prevCluster(s, span.start);
  while (span.end < s.size() && kindAt(s, span.end) == k) span.end = nextCluster(s, span.end);
  return span;
}

std::size_t lineStart(std::string_view s, std::size_t i) {
  i = snapPoint(s, i);
  if (i == 0) return 0;
  const std::size_t nl = s.rfind('\n', i - 1);
  return nl == std::string_view::npos ? 0 : nl + 1;
}

std::size_t lineEnd(std::string_view s, std::size_t i) {
  const std::size_t nl = s.find('\n', snapPoint(s, i));
  return nl == std::string_view::npos ? s.size() : nl;
}

}  // namespace glim::toolkit::text
