#import "A11yBridge.h"

#include <glim/shell/Window.h>

#include <utility>

using glim::shell::A11yElement;
using glim::shell::A11yRole;

@class GlimA11yBridge;

@interface GlimA11yNode : NSAccessibilityElement
@property(nonatomic, weak) NSView* host;
@property(nonatomic, weak) GlimA11yBridge* bridge;
@property(nonatomic) NSRect logical;
@property(nonatomic) int index;
@property(nonatomic) BOOL adjustable;
@property(nonatomic) BOOL focused;
@property(nonatomic, strong) NSMutableArray* kids;
@end

@interface GlimA11yBridge ()
- (void)perform:(int)index command:(glim::shell::A11yCommand)command;
@end

@implementation GlimA11yNode
- (NSRect)accessibilityFrame {
  NSView* host = self.host;
  if (!host.window) {
    return NSZeroRect;
  }
  return [host.window convertRectToScreen:[host convertRect:self.logical toView:nil]];
}
- (NSArray*)accessibilityChildren {
  return self.kids;
}
- (BOOL)accessibilityPerformPress {
  [self.bridge perform:self.index command:glim::shell::A11yCommand::Activate];
  return YES;
}
- (BOOL)isAccessibilityFocused {
  return self.focused;
}
- (void)setAccessibilityFocused:(BOOL)focused {
  if (focused) [self.bridge perform:self.index command:glim::shell::A11yCommand::Focus];
}
- (BOOL)accessibilityPerformIncrement {
  if (!self.adjustable) return NO;
  [self.bridge perform:self.index command:glim::shell::A11yCommand::Increment];
  return YES;
}
- (BOOL)accessibilityPerformDecrement {
  if (!self.adjustable) return NO;
  [self.bridge perform:self.index command:glim::shell::A11yCommand::Decrement];
  return YES;
}
@end

namespace {

NSAccessibilityRole roleOf(A11yRole r) {
  switch (r) {
    case A11yRole::Group:
      return NSAccessibilityGroupRole;
    case A11yRole::Text:
    case A11yRole::Heading:
      return NSAccessibilityStaticTextRole;
    case A11yRole::Button:
      return NSAccessibilityButtonRole;
    case A11yRole::Switch:
      return NSAccessibilityCheckBoxRole;
    case A11yRole::Slider:
      return NSAccessibilitySliderRole;
    case A11yRole::TextField:
      return NSAccessibilityTextFieldRole;
    case A11yRole::Image:
      return NSAccessibilityImageRole;
  }
  return NSAccessibilityGroupRole;
}

NSString* text(const std::string& s) {
  NSString* t = [NSString stringWithUTF8String:s.c_str()];
  return t ? t : @"";
}

}  // namespace

@implementation GlimA11yBridge {
  __weak NSView* view_;
  std::vector<A11yElement> published_;
  NSMutableArray<GlimA11yNode*>* nodes_;
  NSMutableArray* roots_;
  glim::shell::A11yPerform perform_;
  int focused_;
}

- (instancetype)initWithView:(NSView*)view {
  if ((self = [super init])) {
    view_ = view;
    focused_ = -1;
    nodes_ = [NSMutableArray array];
    roots_ = [NSMutableArray array];
  }
  return self;
}

- (NSArray*)roots {
  _active = YES;
  return roots_;
}

- (id)hitTest:(NSPoint)screenPoint {
  _active = YES;
  for (GlimA11yNode* n in [nodes_ reverseObjectEnumerator]) {
    if (NSPointInRect(screenPoint, n.accessibilityFrame)) {
      return n;
    }
  }
  return nil;
}

- (void)setPerform:(glim::shell::A11yPerform)perform {
  perform_ = std::move(perform);
}

- (void)perform:(int)index command:(glim::shell::A11yCommand)command {
  // The OS already holds this focus; the next publish must not echo it.
  if (command == glim::shell::A11yCommand::Focus) focused_ = index;
  if (perform_) {
    perform_(index, command);
  }
}

- (void)publish:(std::vector<A11yElement>)elements {
  if (elements == published_) {
    return;
  }
  published_ = std::move(elements);
  NSView* view = view_;
  while (nodes_.count > published_.size()) {
    [nodes_ removeLastObject];
  }
  while (nodes_.count < published_.size()) {
    GlimA11yNode* n = [[GlimA11yNode alloc] init];
    n.host = view;
    n.bridge = self;
    [nodes_ addObject:n];
  }
  [roots_ removeAllObjects];
  for (std::size_t i = 0; i < published_.size(); ++i) {
    nodes_[i].kids = [NSMutableArray array];
  }
  for (std::size_t i = 0; i < published_.size(); ++i) {
    const A11yElement& e = published_[i];
    GlimA11yNode* n = nodes_[i];
    n.index = static_cast<int>(i);
    n.logical = NSMakeRect(e.frame.origin.x, e.frame.origin.y, e.frame.size.x, e.frame.size.y);
    n.accessibilityRole = roleOf(e.role);
    n.accessibilitySubrole = e.role == A11yRole::Switch ? NSAccessibilitySwitchSubrole : nil;
    n.accessibilityLabel = text(e.label);
    if (e.role == A11yRole::Switch) {
      n.accessibilityValue = @(e.checked ? 1 : 0);
    } else if (!e.value.empty()) {
      n.accessibilityValue = text(e.value);
    } else {
      n.accessibilityValue = nil;
    }
    n.accessibilityEnabled = !e.disabled;
    n.adjustable = e.adjustable;
    n.focused = e.focused;
    const bool nested = e.parent >= 0 && static_cast<std::size_t>(e.parent) < i;
    n.accessibilityParent = nested ? nodes_[static_cast<std::size_t>(e.parent)] : view;
    [(nested ? nodes_[static_cast<std::size_t>(e.parent)].kids : roots_) addObject:n];
  }
  if (view) {
    NSAccessibilityPostNotification(view, NSAccessibilityLayoutChangedNotification);
  }
  const int focused = glim::shell::focusedIndex(published_);
  if (focused != focused_) {
    focused_ = focused;
    if (focused >= 0) {
      NSAccessibilityPostNotification(
          nodes_[static_cast<std::size_t>(focused)], NSAccessibilityFocusedUIElementChangedNotification);
    }
  }
}

@end

namespace glim::shell {

void Window::setA11y(std::vector<A11yElement> elements) {
  [(__bridge GlimA11yBridge*)a11y_ publish:std::move(elements)];
}

void Window::setA11yPerform(A11yPerform perform) {
  [(__bridge GlimA11yBridge*)a11y_ setPerform:std::move(perform)];
}

bool Window::a11yActive() const { return ((__bridge GlimA11yBridge*)a11y_).active; }

}  // namespace glim::shell
