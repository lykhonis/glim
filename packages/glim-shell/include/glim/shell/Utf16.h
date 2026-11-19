#pragma once

#include <glim/shell/TextInput.h>

#include <algorithm>
#include <cstddef>
#include <string>

namespace glim::shell {

namespace detail {

inline std::size_t utf16Of(unsigned char lead) { return (lead >> 4) == 0xF ? 2 : 1; }

inline std::size_t utf8Of(unsigned char lead) {
  if (lead < 0x80) return 1;
  if ((lead >> 5) == 0x6) return 2;
  if ((lead >> 4) == 0xE) return 3;
  return 4;
}

}  // namespace detail

// UTF-16 units before a UTF-8 byte offset: how UIKit, Android and the
// browser count positions in the field's text.
inline std::size_t utf16Units(const std::string& text, std::size_t bytes) {
  std::size_t u = 0;
  for (std::size_t i = 0; i < std::min(bytes, text.size());) {
    const auto lead = static_cast<unsigned char>(text[i]);
    u += detail::utf16Of(lead);
    i += detail::utf8Of(lead);
  }
  return u;
}

// The UTF-8 byte offset of a UTF-16 position, clamped to the text.
inline std::size_t utf16Byte(const std::string& text, std::size_t units) {
  std::size_t i = 0;
  std::size_t u = 0;
  while (i < text.size() && u < units) {
    const auto lead = static_cast<unsigned char>(text[i]);
    u += detail::utf16Of(lead);
    i += detail::utf8Of(lead);
  }
  return std::min(i, text.size());
}

// UTF-16 units [start, start + length) as a byte range.
inline TextInputRange utf16Bytes(const std::string& text, std::size_t start, std::size_t length) {
  const std::size_t a = utf16Byte(text, start);
  return {a, utf16Byte(text, start + length) - a};
}

}  // namespace glim::shell
