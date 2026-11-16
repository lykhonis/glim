#import "A11yBridge.h"

#include <glim/shell/Window.h>

#include <utility>

using glim::shell::A11yElement;
using glim::shell::A11yRole;

@interface GlimA11yBridge ()
- (void)perform:(int)index command:(glim::shell::A11yCommand)command;
@end

@interface GlimA11yNode : UIAccessibilityElement
@property(nonatomic, weak) GlimA11yBridge* bridge;
@property(nonatomic) int index;
@end

@implementation GlimA11yNode
- (BOOL)accessibilityActivate {
  [self.bridge perform:self.index command:glim::shell::A11yCommand::Activate];
  return YES;
}
- (void)accessibilityElementDidBecomeFocused {
  [self.bridge perform:self.index command:glim::shell::A11yCommand::Focus];
}
- (void)accessibilityIncrement {
  [self.bridge perform:self.index command:glim::shell::A11yCommand::Increment];
}
- (void)accessibilityDecrement {
  [self.bridge perform:self.index command:glim::shell::A11yCommand::Decrement];
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
  if (e.adjustable) t |= UIAccessibilityTraitAdjustable;
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
  glim::shell::A11yPerform perform_;
  int focused_;
}

- (instancetype)initWithView:(UIView*)view {
  if ((self = [super init])) {
    view_ = view;
    focused_ = -1;
    nodes_ = [NSMutableArray array];
  }
  return self;
}

- (NSArray*)elements {
  _active = YES;
  return nodes_;
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
  const int focused = glim::shell::focusedIndex(published_);
  // Layout changed with an element moves VoiceOver's cursor onto it.
  const bool moved = focused != focused_ && focused >= 0;
  focused_ = focused;
  UIAccessibilityPostNotification(
      UIAccessibilityLayoutChangedNotification,
      moved ? nodes_[static_cast<std::size_t>(focused)] : nil);
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
