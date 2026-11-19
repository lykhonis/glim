#import "TextView.h"

#include <glim/shell/TextCommands.h>
#include <glim/shell/Utf16.h>

#include <algorithm>
#include <cmath>
#include <utility>

using glim::shell::TextInputOp;
using glim::shell::TextInputRange;
using glim::shell::TextInputState;

@interface GlimTextPosition : UITextPosition
@property(nonatomic, readonly) NSInteger offset;
+ (instancetype)at:(NSInteger)offset;
@end

@implementation GlimTextPosition
+ (instancetype)at:(NSInteger)offset {
  GlimTextPosition* p = [GlimTextPosition new];
  p->_offset = std::max<NSInteger>(0, offset);
  return p;
}
@end

@interface GlimTextRange : UITextRange
+ (instancetype)from:(NSInteger)a to:(NSInteger)b;
+ (instancetype)of:(NSRange)r;
@property(nonatomic, readonly) NSRange range;
@end

@implementation GlimTextRange {
  GlimTextPosition* start_;
  GlimTextPosition* end_;
}
+ (instancetype)from:(NSInteger)a to:(NSInteger)b {
  GlimTextRange* r = [GlimTextRange new];
  r->start_ = [GlimTextPosition at:std::min(a, b)];
  r->end_ = [GlimTextPosition at:std::max(a, b)];
  return r;
}
+ (instancetype)of:(NSRange)r {
  return [self from:r.location to:r.location + r.length];
}
- (UITextPosition*)start {
  return start_;
}
- (UITextPosition*)end {
  return end_;
}
- (BOOL)isEmpty {
  return start_.offset == end_.offset;
}
- (NSRange)range {
  return NSMakeRange(start_.offset, end_.offset - start_.offset);
}
@end

@interface GlimSelectionRect : UITextSelectionRect
+ (instancetype)rect:(CGRect)rect;
@end

@implementation GlimSelectionRect {
  CGRect rect_;
}
+ (instancetype)rect:(CGRect)rect {
  GlimSelectionRect* r = [GlimSelectionRect new];
  r->rect_ = rect;
  return r;
}
- (CGRect)rect {
  return rect_;
}
- (NSWritingDirection)writingDirection {
  return NSWritingDirectionLeftToRight;
}
- (BOOL)containsStart {
  return YES;
}
- (BOOL)containsEnd {
  return YES;
}
- (BOOL)isVertical {
  return NO;
}
@end

namespace {

NSInteger offsetOf(UITextPosition* p) { return ((GlimTextPosition*)p).offset; }

}  // namespace

@implementation GlimTextView {
  UITextInputStringTokenizer* tokenizer_;
}

@synthesize inputDelegate;
@synthesize markedTextStyle;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _textInput = [GlimTextInput new];
    tokenizer_ = [[UITextInputStringTokenizer alloc] initWithTextInput:self];
    self.userInteractionEnabled = NO;
    self.hidden = YES;
#if TARGET_OS_TV
    _keyboardField = [UITextField new];
    _keyboardField.alpha = 0;
    [_keyboardField addTarget:self
                       action:@selector(keyboardEdited)
             forControlEvents:UIControlEventEditingChanged];
#endif
  }
  return self;
}

#if TARGET_OS_TV
- (BOOL)openKeyboard {
  if (!self.textInput.active || !self.superview) return NO;
  if (self.keyboardField.superview != self.superview) [self.superview addSubview:self.keyboardField];
  self.keyboardField.secureTextEntry = self.state.secure;
  self.keyboardField.text = self.state.secure ? @"" : self.string;
  [self.keyboardField becomeFirstResponder];
  return YES;
}

- (void)keyboardEdited {
  [self send:{.op = TextInputOp::Select, .selection = {0, self.state.text.size()}}];
  [self send:{.op = TextInputOp::Insert, .text = glim::shell::detail::utf8(self.keyboardField.text ? self.keyboardField.text : @"")}];
}
#endif

- (const TextInputState&)state {
  return self.textInput.state;
}

- (NSString*)string {
  return @(self.state.text.c_str());
}

- (NSInteger)length {
  return static_cast<NSInteger>(self.string.length);
}

- (void)publish:(TextInputState)state {
  const BOOL wasActive = self.textInput.active;
  [self.inputDelegate selectionWillChange:self];
  [self.inputDelegate textWillChange:self];
  [self.textInput publish:std::move(state)];
  [self.inputDelegate textDidChange:self];
  [self.inputDelegate selectionDidChange:self];
#if TARGET_OS_TV
  if (wasActive && !self.textInput.active && self.keyboardField.isFirstResponder) {
    [self.keyboardField resignFirstResponder];
  }
#else
  if (self.textInput.active && !self.isFirstResponder) {
    [self becomeFirstResponder];
  } else if (wasActive && !self.textInput.active && self.isFirstResponder) {
    [self resignFirstResponder];
  }
#endif
}

- (void)send:(glim::shell::TextInputChange)change {
  [self.textInput send:std::move(change)];
}

- (TextInputRange)bytes:(NSRange)units {
  return glim::shell::detail::bytes(self.state.text, units);
}

- (BOOL)canBecomeFirstResponder {
  return self.textInput.active;
}

#pragma mark UIKeyInput

- (BOOL)hasText {
  return !self.state.text.empty();
}

- (void)insertText:(NSString*)text {
  if ([text isEqualToString:@"\n"]) return;
  [self send:{.op = TextInputOp::Insert, .text = glim::shell::detail::utf8(text)}];
}

- (void)deleteBackward {
  [self send:{.op = TextInputOp::DeleteBackward}];
}

#if !TARGET_OS_TV
- (UITextAutocorrectionType)autocorrectionType {
  return self.state.secure ? UITextAutocorrectionTypeNo : UITextAutocorrectionTypeDefault;
}
#endif

- (BOOL)isSecureTextEntry {
  return self.state.secure;
}

#pragma mark Text and ranges

- (NSString*)textInRange:(UITextRange*)range {
  NSString* s = self.string;
  const NSRange r = NSIntersectionRange(((GlimTextRange*)range).range, NSMakeRange(0, s.length));
  return [s substringWithRange:r];
}

- (void)replaceRange:(UITextRange*)range withText:(NSString*)text {
  [self send:{.op = TextInputOp::Select, .selection = [self bytes:((GlimTextRange*)range).range]}];
  [self send:{.op = TextInputOp::Insert, .text = glim::shell::detail::utf8(text)}];
}

- (UITextRange*)selectedTextRange {
  if (!self.textInput.active) return nil;
  return [GlimTextRange of:[self.textInput utf16:self.state.selection]];
}

- (void)setSelectedTextRange:(UITextRange*)range {
  if (!range) return;
  [self send:{.op = TextInputOp::Select, .selection = [self bytes:((GlimTextRange*)range).range]}];
}

- (UITextRange*)markedTextRange {
  const auto& marked = self.state.marked;
  return marked ? [GlimTextRange of:[self.textInput utf16:*marked]] : nil;
}

- (void)setMarkedText:(NSString*)text selectedRange:(NSRange)selected {
  const std::string utf8 = glim::shell::detail::utf8(text ? text : @"");
  [self send:{
               .op = TextInputOp::Mark,
               .text = utf8,
               .selection = glim::shell::detail::bytes(utf8, selected),
             }];
}

- (void)unmarkText {
  [self send:{.op = TextInputOp::Unmark}];
}

- (UITextPosition*)beginningOfDocument {
  return [GlimTextPosition at:0];
}

- (UITextPosition*)endOfDocument {
  return [GlimTextPosition at:self.length];
}

- (UITextRange*)textRangeFromPosition:(UITextPosition*)from toPosition:(UITextPosition*)to {
  return [GlimTextRange from:offsetOf(from) to:offsetOf(to)];
}

- (UITextPosition*)positionFromPosition:(UITextPosition*)position offset:(NSInteger)offset {
  const NSInteger at = offsetOf(position) + offset;
  return at < 0 || at > self.length ? nil : [GlimTextPosition at:at];
}

- (UITextPosition*)positionFromPosition:(UITextPosition*)position
                            inDirection:(UITextLayoutDirection)direction
                                 offset:(NSInteger)offset {
  const bool back = direction == UITextLayoutDirectionLeft || direction == UITextLayoutDirectionUp;
  return [self positionFromPosition:position offset:back ? -offset : offset];
}

- (NSComparisonResult)comparePosition:(UITextPosition*)a toPosition:(UITextPosition*)b {
  const NSInteger x = offsetOf(a);
  const NSInteger y = offsetOf(b);
  return x < y ? NSOrderedAscending : (x > y ? NSOrderedDescending : NSOrderedSame);
}

- (NSInteger)offsetFromPosition:(UITextPosition*)from toPosition:(UITextPosition*)to {
  return offsetOf(to) - offsetOf(from);
}

- (id<UITextInputTokenizer>)tokenizer {
  return tokenizer_;
}

- (UITextPosition*)positionWithinRange:(UITextRange*)range
                   farthestInDirection:(UITextLayoutDirection)direction {
  const bool back = direction == UITextLayoutDirectionLeft || direction == UITextLayoutDirectionUp;
  return back ? range.start : range.end;
}

- (UITextRange*)characterRangeByExtendingPosition:(UITextPosition*)position
                                      inDirection:(UITextLayoutDirection)direction {
  const bool back = direction == UITextLayoutDirectionLeft || direction == UITextLayoutDirectionUp;
  const NSInteger at = offsetOf(position);
  return back ? [GlimTextRange from:0 to:at] : [GlimTextRange from:at to:self.length];
}

- (NSWritingDirection)baseWritingDirectionForPosition:(UITextPosition*)position
                                          inDirection:(UITextStorageDirection)direction {
  (void)position;
  (void)direction;
  return NSWritingDirectionLeftToRight;
}

- (void)setBaseWritingDirection:(NSWritingDirection)direction forRange:(UITextRange*)range {
  (void)direction;
  (void)range;
}

#pragma mark Geometry

// One line per field: every range anchors at the published caret, which
// is what the candidate bar and the loupe need.
- (CGRect)caretRect {
  const glim::Rect c = self.state.caret;
  return CGRectMake(c.origin.x, c.origin.y, c.size.x, c.size.y);
}

// The painted x of a UTF-16 position: the last stop at or before it.
- (CGFloat)xAt:(NSInteger)units {
  const auto& stops = self.state.stops;
  if (stops.empty()) return [self caretRect].origin.x;
  const std::size_t byte = glim::shell::utf16Byte(self.state.text, static_cast<std::size_t>(units));
  CGFloat x = stops.front().x;
  for (const auto& s : stops) {
    if (s.at > byte) break;
    x = s.x;
  }
  return x;
}

- (CGRect)spanFrom:(NSInteger)a to:(NSInteger)b {
  const CGRect line = [self caretRect];
  const CGFloat x0 = [self xAt:a];
  return CGRectMake(x0, line.origin.y, [self xAt:b] - x0, line.size.height);
}

- (CGRect)firstRectForRange:(UITextRange*)range {
  const NSRange r = ((GlimTextRange*)range).range;
  return [self spanFrom:r.location to:r.location + r.length];
}

- (CGRect)caretRectForPosition:(UITextPosition*)position {
  const CGRect line = [self caretRect];
  return CGRectMake([self xAt:offsetOf(position)], line.origin.y, line.size.width, line.size.height);
}

- (NSArray<UITextSelectionRect*>*)selectionRectsForRange:(UITextRange*)range {
  return @[ [GlimSelectionRect rect:[self firstRectForRange:range]] ];
}

// The stop nearest x within [lo, hi], as a UTF-16 position.
- (NSInteger)nearest:(CGFloat)x from:(NSInteger)lo to:(NSInteger)hi {
  const std::string& text = self.state.text;
  NSInteger best = lo;
  CGFloat gap = CGFLOAT_MAX;
  for (const auto& s : self.state.stops) {
    const auto u = static_cast<NSInteger>(glim::shell::utf16Units(text, s.at));
    if (u < lo || u > hi) continue;
    if (std::abs(s.x - x) < gap) {
      gap = std::abs(s.x - x);
      best = u;
    }
  }
  return best;
}

- (UITextPosition*)closestPositionToPoint:(CGPoint)point {
  return [GlimTextPosition at:[self nearest:point.x from:0 to:self.length]];
}

- (UITextPosition*)closestPositionToPoint:(CGPoint)point withinRange:(UITextRange*)range {
  const NSRange r = ((GlimTextRange*)range).range;
  return [GlimTextPosition at:[self nearest:point.x from:r.location to:r.location + r.length]];
}

- (UITextRange*)characterRangeAtPoint:(CGPoint)point {
  const NSInteger at = [self nearest:point.x from:0 to:self.length];
  return [GlimTextRange from:at to:std::min(at + 1, self.length)];
}

#pragma mark Edit commands

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (action == @selector(copy:) || action == @selector(cut:)) return self.textInput.canCopy;
  if (action == @selector(paste:)) return self.textInput.canPaste;
  if (action == @selector(selectAll:)) return self.textInput.active;
  return [super canPerformAction:action withSender:sender];
}

- (void)copy:(id)sender {
  (void)sender;
  [self.textInput copySelection];
}

- (void)cut:(id)sender {
  (void)sender;
  [self.textInput cutSelection];
}

- (void)paste:(id)sender {
  (void)sender;
  [self.textInput paste];
}

- (void)selectAll:(id)sender {
  (void)sender;
  [self send:{.op = TextInputOp::SelectAll}];
}

// Hardware keyboard commands UIKit does not route through UITextInput.
- (NSArray<UIKeyCommand*>*)keyCommands {
  const auto command = [](NSString* input, UIKeyModifierFlags mods, NSString* name) {
    UIKeyCommand* c = [UIKeyCommand commandWithTitle:name
                                               image:nil
                                              action:@selector(textCommand:)
                                               input:input
                                       modifierFlags:mods
                                        propertyList:name];
    c.wantsPriorityOverSystemBehavior = YES;
    return c;
  };
  const UIKeyModifierFlags cmd = UIKeyModifierCommand;
  return @[
    command(@"z", cmd, @"undo:"),
    command(@"z", cmd | UIKeyModifierShift, @"redo:"),
    command(UIKeyInputDelete, 0, @"deleteForward:"),
    command(UIKeyInputDelete, UIKeyModifierAlternate, @"deleteWordForward:"),
    command(@"\b", UIKeyModifierAlternate, @"deleteWordBackward:"),
    command(@"\b", cmd, @"deleteToBeginningOfLine:"),
  ];
}

- (void)textCommand:(UIKeyCommand*)sender {
  if (auto change = glim::shell::textCommand(((NSString*)sender.propertyList).UTF8String)) {
    [self send:*change];
  }
}

@end
