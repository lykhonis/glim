#import "TextInputBridge.h"

#include <algorithm>
#include <utility>

using glim::shell::TextInputChange;
using glim::shell::TextInputRange;
using glim::shell::TextInputState;

namespace glim::shell::detail {

namespace {

std::size_t unitsOf(unsigned char lead) {
  if (lead < 0x80) return 1;
  return (lead >> 4) == 0xF ? 2 : 1;
}

std::size_t pointBytes(unsigned char lead) {
  if (lead < 0x80) return 1;
  if ((lead >> 5) == 0x6) return 2;
  if ((lead >> 4) == 0xE) return 3;
  return 4;
}

std::size_t units(const std::string& text, std::size_t bytes) {
  std::size_t u = 0;
  for (std::size_t i = 0; i < std::min(bytes, text.size());) {
    const auto lead = static_cast<unsigned char>(text[i]);
    u += unitsOf(lead);
    i += pointBytes(lead);
  }
  return u;
}

std::size_t byteAt(const std::string& text, std::size_t units) {
  std::size_t i = 0;
  std::size_t u = 0;
  while (i < text.size() && u < units) {
    const auto lead = static_cast<unsigned char>(text[i]);
    u += unitsOf(lead);
    i += pointBytes(lead);
  }
  return std::min(i, text.size());
}

}  // namespace

std::string utf8(id string) {
  NSString* s = [string isKindOfClass:[NSAttributedString class]]
                  ? ((NSAttributedString*)string).string
                  : (NSString*)string;
  const char* c = s.UTF8String;
  return c ? std::string(c) : std::string();
}

TextInputRange bytes(const std::string& text, NSRange r) {
  const std::size_t a = byteAt(text, r.location);
  const std::size_t b = byteAt(text, r.location + r.length);
  return {a, b - a};
}

}  // namespace glim::shell::detail

@implementation GlimTextInput {
  TextInputState state_;
  glim::shell::TextInputCallback callback_;
}

- (BOOL)active {
  return state_.active;
}

- (const TextInputState&)state {
  return state_;
}

- (BOOL)publish:(TextInputState)state {
  const glim::Rect before = state_.caret;
  state_ = std::move(state);
  return before.origin.x != state_.caret.origin.x || before.origin.y != state_.caret.origin.y;
}

- (void)setCallback:(glim::shell::TextInputCallback)callback {
  callback_ = std::move(callback);
}

- (void)send:(TextInputChange)change {
  if (callback_ && state_.active) callback_(change);
}

- (NSRange)utf16:(TextInputRange)range {
  const std::size_t a = glim::shell::detail::units(state_.text, range.start);
  const std::size_t b = glim::shell::detail::units(state_.text, range.start + range.length);
  return NSMakeRange(a, b - a);
}

@end

