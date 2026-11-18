#pragma once

#include <cstddef>
#include <string_view>

namespace glim::toolkit::text {

// Boundaries in UTF-8 text, as byte offsets. Clusters follow the extended
// grapheme rules a caret needs (combining marks, variation selectors,
// emoji modifiers, ZWJ sequences, flag pairs, CRLF); Hangul and Indic
// conjuncts wait for the shaping stack (I1). Lines are hard lines ('\n').

// The code point starting at i, and where the next one starts.
char32_t decodeAt(std::string_view s, std::size_t i, std::size_t* next = nullptr);
std::size_t snapPoint(std::string_view s, std::size_t i);

std::size_t nextCluster(std::string_view s, std::size_t i);
std::size_t prevCluster(std::string_view s, std::size_t i);
std::size_t clusterCount(std::string_view s);

bool isWordPoint(char32_t c);
// Past the end of the next word, or back to the start of the previous one.
std::size_t nextWordEnd(std::string_view s, std::size_t i);
std::size_t prevWordStart(std::string_view s, std::size_t i);
// The run at i of one kind: a word, spaces, or other symbols.
struct Span {
  std::size_t start = 0;
  std::size_t end = 0;
};
Span runAt(std::string_view s, std::size_t i);

std::size_t lineStart(std::string_view s, std::size_t i);
std::size_t lineEnd(std::string_view s, std::size_t i);

}  // namespace glim::toolkit::text
