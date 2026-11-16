#import "A11yBridge.h"

#include <glim/shell/Window.h>

#include <utility>

using glim::shell::A11yElement;
using glim::shell::A11yRole;

@interface GlimA11yBridge ()
- (void)activate:(int)index;
@end

@interface GlimA11yNode : UIAccessibilityElement
@property(nonatomic, weak) GlimA11yBridge* bridge;
@property(nonatomic) int index;
@end

@implementation GlimA11yNode
- (BOOL)accessibilityActivate {
  [self.bridge activate:self.index];
  return YES;
}
@end

namespace {

UIAccessibilityTraits traitsOf(const A11yElement& e) {
  UIAccessibilityTraits t = UIAccessibilityTraitNone;
  switch (e.role) {
    case A11yRole::Group:
      break;
    case A11yRole::Text:
      t = UIAccessibilityTraitStaticText;
      break;
    case A11yRole::Heading:
      t = UIAccessibilityTraitHeader;
      break;
    case A11yRole::Button:
    case A11yRole::Switch:
    case A11yRole::TextField:
      t = UIAccessibilityTraitButton;
      break;
    case A11yRole::Slider:
      t = UIAccessibilityTraitAdjustable;
      break;
    case A11yRole::Image:
      t = UIAccessibilityTraitImage;
      break;
  }
  if (e.checked) t |= UIAccessibilityTraitSelected;
  if (e.disabled) t |= UIAccessibilityTraitNotEnabled;
  return t;
}

NSString* text(const std::string& s) {
  NSString* t = [NSString stringWithUTF8String:s.c_str()];
  return t ? t : @"";
}

}  // namespace

@implementation GlimA11yBridge {
  __weak UIView* view_;
  std::vector<A11yElement> published_;
  NSMutableArray<GlimA11yNode*>* nodes_;
  glim::shell::A11yActivate activate_;
}

- (instancetype)initWithView:(UIView*)view {
  if ((self = [super init])) {
    view_ = view;
    nodes_ = [NSMutableArray array];
  }
  return self;
}

- (NSArray*)elements {
  _active = YES;
  return nodes_;
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
  UIView* view = view_;
  while (nodes_.count > published_.size()) {
    [nodes_ removeLastObject];
  }
  while (nodes_.count < published_.size()) {
    GlimA11yNode* n = [[GlimA11yNode alloc] initWithAccessibilityContainer:view];
    n.bridge = self;
    [nodes_ addObject:n];
  }
  for (std::size_t i = 0; i < published_.size(); ++i) {
    const A11yElement& e = published_[i];
    GlimA11yNode* n = nodes_[i];
    n.index = static_cast<int>(i);
    n.isAccessibilityElement = e.role != A11yRole::Group || !e.label.empty();
    n.accessibilityFrameInContainerSpace =
        CGRectMake(e.frame.origin.x, e.frame.origin.y, e.frame.size.x, e.frame.size.y);
    n.accessibilityLabel = text(e.label);
    n.accessibilityValue = e.value.empty() ? nil : text(e.value);
    n.accessibilityTraits = traitsOf(e);
  }
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification, nil);
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
