// Runs the UIKit a11y bridge on the simulator (xcrun simctl spawn): element
// frames and traits, activate / increment / decrement, and focus both ways.
#import "../src/A11yBridge.h"

#include <cstdlib>
#include <iostream>
#include <vector>

using glim::shell::A11yCommand;
using glim::shell::A11yElement;
using glim::shell::A11yRole;

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

}  // namespace

int main() {
  @autoreleasepool {
    UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 400, 300)];
    GlimA11yBridge* bridge = [[GlimA11yBridge alloc] initWithView:view];
    std::vector<std::pair<int, A11yCommand>> got;
    [bridge setPerform:[&](int index, A11yCommand c) { got.emplace_back(index, c); }];

    expect(!bridge.active, "inactive until UIKit asks");
    std::vector<A11yElement> list{
      {.role = A11yRole::Button, .label = "Play", .frame = {{10, 20}, {80, 40}}},
      {.role = A11yRole::Slider,
       .label = "Volume",
       .value = "3",
       .frame = {{10, 80}, {200, 30}},
       .adjustable = true,
       .focused = true},
    };
    [bridge publish:list];
    NSArray* elements = [bridge elements];
    expect(bridge.active && elements.count == 2, "two elements, bridge active");
    UIAccessibilityElement* play = elements[0];
    UIAccessibilityElement* volume = elements[1];
    expect([play.accessibilityLabel isEqualToString:@"Play"], "label");
    expect(CGRectEqualToRect(play.accessibilityFrameInContainerSpace, CGRectMake(10, 20, 80, 40)),
           "frame in container space");
    expect((play.accessibilityTraits & UIAccessibilityTraitButton) != 0, "button trait");
    expect((volume.accessibilityTraits & UIAccessibilityTraitAdjustable) != 0, "adjustable trait");
    expect([volume.accessibilityValue isEqualToString:@"3"], "value");

    expect([play accessibilityActivate], "activate accepted");
    [volume accessibilityIncrement];
    [volume accessibilityDecrement];
    [play accessibilityElementDidBecomeFocused];
    const std::vector<std::pair<int, A11yCommand>> want{
      {0, A11yCommand::Activate},
      {1, A11yCommand::Increment},
      {1, A11yCommand::Decrement},
      {0, A11yCommand::Focus},
    };
    expect(got == want, "activate, increment, decrement and OS focus reach the host in order");
  }
  if (failures == 0) std::cout << "ios a11y bridge: elements, actions and focus ok\n";
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
