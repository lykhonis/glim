#include "Tokenizer.h"

#include <cctype>

namespace glimlint {
namespace {

bool identStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }

bool identPart(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

}  // namespace

std::vector<Token> tokenize(std::string_view s) {
  std::vector<Token> out;
  std::size_t i = 0;
  int line = 1;
  bool lineStart = true;

  auto skipQuoted = [&](char q) {
    ++i;
    while (i < s.size() && s[i] != q) {
      if (s[i] == '\\') ++i;
      if (i < s.size() && s[i] == '\n') ++line;
      ++i;
    }
    ++i;
  };

  while (i < s.size()) {
    char c = s[i];
    if (c == '\n') {
      ++line;
      ++i;
      lineStart = true;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(c))) {
      ++i;
      continue;
    }
    if (c == '#' && lineStart) {
      while (i < s.size() && s[i] != '\n') {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '\n') {
          ++line;
          ++i;
        }
        ++i;
      }
      continue;
    }
    lineStart = false;
    if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
      while (i < s.size() && s[i] != '\n') ++i;
      continue;
    }
    if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
      i += 2;
      while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) {
        if (s[i] == '\n') ++line;
        ++i;
      }
      i += 2;
      continue;
    }
    if (identStart(c)) {
      std::size_t b = i;
      while (i < s.size() && identPart(s[i])) ++i;
      std::string_view word = s.substr(b, i - b);
      if (word == "R" && i < s.size() && s[i] == '"') {
        std::size_t open = s.find('(', i);
        std::string close = ")" + std::string(s.substr(i + 1, open - i - 1)) + "\"";
        std::size_t end = s.find(close, open);
        end = end == std::string_view::npos ? s.size() : end + close.size();
        for (std::size_t k = i; k < end; ++k)
          if (s[k] == '\n') ++line;
        i = end;
        out.push_back({Kind::Literal, "\"\"", line});
        continue;
      }
      out.push_back({Kind::Ident, std::string(word), line});
      continue;
    }
    if (std::isdigit(static_cast<unsigned char>(c))) {
      while (i < s.size() && (identPart(s[i]) || s[i] == '.' || s[i] == '\'')) ++i;
      out.push_back({Kind::Literal, "0", line});
      continue;
    }
    if (c == '"' || c == '\'') {
      skipQuoted(c);
      out.push_back({Kind::Literal, "\"\"", line});
      continue;
    }
    if (i + 1 < s.size() && ((c == '-' && s[i + 1] == '>') || (c == ':' && s[i + 1] == ':'))) {
      out.push_back({Kind::Punct, std::string(s.substr(i, 2)), line});
      i += 2;
      continue;
    }
    out.push_back({Kind::Punct, std::string(1, c), line});
    ++i;
  }
  return out;
}

}  // namespace glimlint
