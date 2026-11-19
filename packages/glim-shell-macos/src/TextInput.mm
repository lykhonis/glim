#import "GlimView.h"

#include <glim/shell/TextCommands.h>

#include <algorithm>

using glim::shell::TextInputChange;
using glim::shell::TextInputOp;
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

// NSTextInputClient: the IME and key bindings edit the focused field
// through these; every edit goes back to the host as a TextInputChange.
@implementation GlimView (TextInput)

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
  (void)replacementRange;
  [self.textInput send:{.op = TextInputOp::Insert, .text = glim::shell::detail::utf8(string)}];
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange {
  (void)replacementRange;
  const std::string text = glim::shell::detail::utf8(string);
  [self.textInput send:{.op = TextInputOp::Mark,
                        .text = text,
                        .selection = glim::shell::detail::bytes(text, selectedRange)}];
}

- (void)unmarkText {
  [self.textInput send:{.op = TextInputOp::Unmark}];
}

- (void)doCommandBySelector:(SEL)selector {
  if (auto change = glim::shell::textCommand(NSStringFromSelector(selector).UTF8String)) {
    [self.textInput send:*change];
  } else if (selector == @selector(insertNewline:)) {
    glim::shell::Event e(glim::shell::EventType::KeyDown);
    e.setKey(glim::shell::Key::Enter);
    [self dispatchEvent:e];
  } else if (selector == @selector(cancelOperation:)) {
    glim::shell::Event e(glim::shell::EventType::KeyDown);
    e.setKey(glim::shell::Key::Escape);
    [self dispatchEvent:e];
  }
}

// Edit menu commands on the focused field; disabled when none is focused,
// so the menu passes them on.
- (void)selectAll:(id)sender {
  (void)sender;
  [self.textInput send:{.op = TextInputOp::SelectAll}];
}

- (void)copy:(id)sender {
  (void)sender;
  const auto& s = self.textInput.state;
  if (s.secure) return;
  const std::string picked = s.text.substr(s.selection.start, s.selection.length);
  NSPasteboard* board = NSPasteboard.generalPasteboard;
  [board clearContents];
  [board setString:@(picked.c_str()) forType:NSPasteboardTypeString];
}

- (void)cut:(id)sender {
  if (self.textInput.state.secure) return;
  [self copy:sender];
  [self.textInput send:{.op = TextInputOp::DeleteBackward}];
}

- (void)undo:(id)sender {
  (void)sender;
  [self.textInput send:{.op = TextInputOp::Undo}];
}

- (void)redo:(id)sender {
  (void)sender;
  [self.textInput send:{.op = TextInputOp::Redo}];
}

- (void)paste:(id)sender {
  (void)sender;
  NSString* text = [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];
  if (text) [self.textInput send:{.op = TextInputOp::Insert, .text = glim::shell::detail::utf8(text)}];
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
  const SEL a = item.action;
  if (a == @selector(copy:) || a == @selector(cut:)) {
    const auto& s = self.textInput.state;
    return self.textInput.active && !s.secure && s.selection.length > 0;
  }
  if (a == @selector(paste:)) {
    return self.textInput.active &&
           [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString] != nil;
  }
  if (a == @selector(selectAll:) || a == @selector(undo:) || a == @selector(redo:)) {
    return self.textInput.active;
  }
  return YES;
}

- (BOOL)hasMarkedText {
  return self.textInput.state.marked.has_value();
}

- (NSRange)markedRange {
  const auto& marked = self.textInput.state.marked;
  return marked ? [self.textInput utf16:*marked] : NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange {
  if (!self.textInput.active) return NSMakeRange(NSNotFound, 0);
  return [self.textInput utf16:self.textInput.state.selection];
}

- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText {
  return @[];
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:(NSRangePointer)actualRange {
  NSString* text = @(self.textInput.state.text.c_str());
  const NSRange whole = NSMakeRange(0, text.length);
  const NSRange r = NSIntersectionRange(whole, range);
  if (actualRange) *actualRange = r;
  return [[NSAttributedString alloc] initWithString:[text substringWithRange:r]];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
  (void)point;
  return NSNotFound;
}

// The candidate window sits under the caret, in screen coordinates.
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
  if (actualRange) *actualRange = range;
  const glim::Rect c = self.textInput.state.caret;
  const NSRect inView = NSMakeRect(c.origin.x, c.origin.y, c.size.x, c.size.y);
  const NSRect inWindow = [self convertRect:inView toView:nil];
  return self.window ? [self.window convertRectToScreen:inWindow] : inWindow;
}

@end

namespace glim::shell {

void Window::setTextInput(TextInputState state) {
  GlimView* view = (__bridge GlimView*)view_;
  const BOOL wasActive = view.textInput.active;
  const BOOL moved = [view.textInput publish:std::move(state)];
  if (wasActive && !view.textInput.active) {
    [view.inputContext discardMarkedText];
  } else if (moved) {
    [view.inputContext invalidateCharacterCoordinates];
  }
}

void Window::setTextInputCallback(TextInputCallback callback) {
  [((__bridge GlimView*)view_).textInput setCallback:std::move(callback)];
}

}  // namespace glim::shell
