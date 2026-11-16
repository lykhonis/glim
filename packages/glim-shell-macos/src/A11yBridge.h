#pragma once

#import <AppKit/AppKit.h>

#include <glim/shell/A11y.h>

#include <vector>

// The view's accessibility children, rebuilt only when the published list
// changes. Asking for the children marks the bridge active.
@interface GlimA11yBridge : NSObject
@property(nonatomic, readonly) BOOL active;
- (instancetype)initWithView:(NSView*)view;
- (NSArray*)roots;
- (id)hitTest:(NSPoint)screenPoint;
- (void)publish:(std::vector<glim::shell::A11yElement>)elements;
- (void)setPerform:(glim::shell::A11yPerform)perform;
@end
