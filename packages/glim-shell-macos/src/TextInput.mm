#import "GlimView.h"

#include <glim/shell/TextCommands.h>

#include <algorithm>

using glim::shell::TextInputChange;
using glim::shell::TextInputOp;
using glim::shell::TextInputRange;
using glim::shell::TextInputState;

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
  [self.textInput copySelection];
}

- (void)cut:(id)sender {
  (void)sender;
  [self.textInput cutSelection];
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
  [self.textInput paste];
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
  const SEL a = item.action;
  if (a == @selector(copy:) || a == @selector(cut:)) return self.textInput.canCopy;
  if (a == @selector(paste:)) return self.textInput.canPaste;
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
