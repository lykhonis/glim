#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace glimlint {

enum class Kind { Ident, Punct, Literal };

struct Token {
  Kind kind;
  std::string text;
  int line;
};

// Forgiving C++ tokenizer: drops comments and preprocessor lines,
// collapses string/char/number literals, keeps `->` and `::` whole.
std::vector<Token> tokenize(std::string_view src);

}  // namespace glimlint
