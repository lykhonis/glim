#pragma once

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

#include <glim/math.h>

#include <cstdint>

// Seats under a 3D transform (K88): each native view sits in a host view
// whose layer carries the transform, so Core Animation draws it projected.
// AppKit hit testing ignores layer transforms, so the root view asks
// route: first; it unprojects the point and forwards the event.
@interface GlimSlotLayers : NSObject
- (instancetype)initWithRoot:(NSView*)root;
- (void)project:(std::uint32_t)id
         native:(NSView*)native
          local:(glim::Rect)local
      transform:(const glim::Mat4&)transform;
// Back to a flat sibling of the surface; no-op when not projected.
- (void)flatten:(std::uint32_t)id native:(NSView*)native;
- (void)remove:(std::uint32_t)id;
// True when a projected seat took the event.
- (BOOL)route:(NSEvent*)event;
@end
