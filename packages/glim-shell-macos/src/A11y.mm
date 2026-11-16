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
@property(nonatomic, strong) NSMutableArray* kids;
@end

@interface GlimA11yBridge ()
- (void)activate:(int)index;
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
  [self.bridge activate:self.index];
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
  glim::shell::A11yActivate activate_;
}

- (instancetype)initWithView:(NSView*)view {
  if ((self = [super init])) {
    view_ = view;
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

- (void)setActivate:(glim::shell::A11yActivate)activate {
  activate_ = std::move(activate);
}

- (void)activate:(int)index {
  if (activate_) {
    activate_(index);
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
    const bool nested = e.parent >= 0 && static_cast<std::size_t>(e.parent) < i;
    n.accessibilityParent = nested ? nodes_[static_cast<std::size_t>(e.parent)] : view;
    [(nested ? nodes_[static_cast<std::size_t>(e.parent)].kids : roots_) addObject:n];
  }
  if (view) {
    NSAccessibilityPostNotification(view, NSAccessibilityLayoutChangedNotification);
  }
}

@end

namespace glim::shell {

void Window::setA11y(std::vector<A11yElement> elements) {
  [(__bridge GlimA11yBridge*)a11y_ publish:std::move(elements)];
}

void Window::setA11yActivate(A11yActivate activate) {
  [(__bridge GlimA11yBridge*)a11y_ setActivate:std::move(activate)];
}

bool Window::a11yActive() const { return ((__bridge GlimA11yBridge*)a11y_).active; }

}  // namespace glim::shell
