#import "TextInputBridge.h"

#include <glim/shell/Clipboard.h>
#include <glim/shell/Utf16.h>

#include <algorithm>
#include <utility>

using glim::shell::TextInputChange;
using glim::shell::TextInputRange;
using glim::shell::TextInputState;

namespace glim::shell::detail {

std::string utf8(id string) {
  NSString* s = [string isKindOfClass:[NSAttributedString class]]
                  ? ((NSAttributedString*)string).string
                  : (NSString*)string;
  const char* c = s.UTF8String;
  return c ? std::string(c) : std::string();
}

TextInputRange bytes(const std::string& text, NSRange r) {
  return utf16Bytes(text, r.location, r.length);
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
  const std::size_t a = glim::shell::utf16Units(state_.text, range.start);
  const std::size_t b = glim::shell::utf16Units(state_.text, range.start + range.length);
  return NSMakeRange(a, b - a);
}

- (BOOL)canCopy {
  return state_.active && !state_.secure && state_.selection.length > 0;
}

- (BOOL)canPaste {
  return state_.active && glim::shell::readClipboard().status == glim::shell::ClipboardStatus::Ok;
}

- (void)copySelection {
  if (!self.canCopy) return;
  glim::shell::writeClipboard(
    std::string_view(state_.text).substr(state_.selection.start, state_.selection.length)
  );
}

- (void)cutSelection {
  if (!self.canCopy) return;
  [self copySelection];
  [self send:{.op = glim::shell::TextInputOp::DeleteBackward}];
}

- (void)paste {
  auto pasted = glim::shell::readClipboard();
  if (pasted.status == glim::shell::ClipboardStatus::Ok) {
    [self send:{.op = glim::shell::TextInputOp::Insert, .text = std::move(pasted.text)}];
  }
}

@end

