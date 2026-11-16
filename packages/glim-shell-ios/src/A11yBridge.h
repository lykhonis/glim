#pragma once

#import <UIKit/UIKit.h>

#include <glim/shell/A11y.h>

#include <vector>

// The view's accessibility elements, flat (UIKit containers read in order),
// rebuilt only when the published list changes. Asking marks it active.
@interface GlimA11yBridge : NSObject
@property(nonatomic, readonly) BOOL active;
- (instancetype)initWithView:(UIView*)view;
- (NSArray*)elements;
- (void)publish:(std::vector<glim::shell::A11yElement>)elements;
- (void)setPerform:(glim::shell::A11yPerform)perform;
@end
