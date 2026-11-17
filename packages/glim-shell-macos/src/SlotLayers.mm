#import "SlotLayers.h"

#include <glim/shell/SlotLayer.h>

#include <unordered_map>

namespace {

struct Projected {
  NSView* host = nil;
  NSView* native = nil;
  glim::Rect local{};
  glim::Mat4 transform = glim::Mat4::identity();
};

}  // namespace

// Flipped like the root, so local coordinates read y-down. It never takes
// hits itself: GlimSlotLayers routes them through the projection.
@interface GlimSlotHost : NSView
@end

@implementation GlimSlotHost
- (BOOL)isFlipped {
  return YES;
}
- (NSView*)hitTest:(NSPoint)point {
  (void)point;
  return nil;
}
@end

@implementation GlimSlotLayers {
  __weak NSView* root_;
  std::unordered_map<std::uint32_t, Projected> projected_;
  std::uint32_t captured_;
}

- (instancetype)initWithRoot:(NSView*)root {
  if ((self = [super init])) {
    root_ = root;
    captured_ = 0;
  }
  return self;
}

- (void)project:(std::uint32_t)id
         native:(NSView*)native
          local:(glim::Rect)local
      transform:(const glim::Mat4&)transform {
  NSView* root = root_;
  if (!root || !native) return;
  Projected& p = projected_[id];
  if (!p.host) {
    p.host = [[GlimSlotHost alloc] initWithFrame:NSZeroRect];
    p.host.wantsLayer = YES;
    [root addSubview:p.host positioned:NSWindowAbove relativeTo:native.superview == root ? native : nil];
  }
  if (native.superview != p.host) {
    [native removeFromSuperview];
    [p.host addSubview:native];
  }
  p.native = native;
  p.local = local;
  p.transform = transform;
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  p.host.layer.transform = CATransform3DIdentity;
  p.host.frame = NSMakeRect(local.origin.x, local.origin.y, local.size.x, local.size.y);
  native.frame = p.host.bounds;
  p.host.layer.anchorPoint = CGPointZero;
  p.host.layer.position = CGPointMake(local.origin.x, local.origin.y);
  p.host.layer.transform =
    glim::shell::toCoreAnimation<CATransform3D>(glim::shell::slotLayerTransform(local, transform));
  [CATransaction commit];
}

- (void)flatten:(std::uint32_t)id native:(NSView*)native {
  auto it = projected_.find(id);
  if (it == projected_.end()) return;
  NSView* root = root_;
  if (native && native.superview == it->second.host && root) {
    [native removeFromSuperview];
    [root addSubview:native positioned:NSWindowAbove relativeTo:it->second.host];
  }
  [it->second.host removeFromSuperview];
  if (captured_ == id) captured_ = 0;
  projected_.erase(it);
}

- (void)remove:(std::uint32_t)id {
  auto it = projected_.find(id);
  if (it == projected_.end()) return;
  [it->second.native removeFromSuperview];
  [it->second.host removeFromSuperview];
  if (captured_ == id) captured_ = 0;
  projected_.erase(it);
}

// The topmost projected seat under the point, and the point in its space.
- (std::uint32_t)seatAt:(glim::Vec2)p local:(glim::Vec2*)out {
  NSView* root = root_;
  for (NSView* v in [root.subviews reverseObjectEnumerator]) {
    for (auto& [id, seat] : projected_) {
      if (seat.host != v) continue;
      const std::optional<glim::Vec2> q = glim::unprojectPlane(seat.transform, p);
      const glim::Rect& r = seat.local;
      if (q && q->x >= r.origin.x && q->x < r.origin.x + r.size.x && q->y >= r.origin.y &&
          q->y < r.origin.y + r.size.y) {
        *out = *q;
        return id;
      }
    }
  }
  return 0;
}

- (BOOL)route:(NSEvent*)event {
  NSView* root = root_;
  if (!root || projected_.empty()) return NO;
  const NSPoint at = [root convertPoint:event.locationInWindow fromView:nil];
  const glim::Vec2 p{static_cast<float>(at.x), static_cast<float>(at.y)};
  const NSEventType type = event.type;
  const bool down = type == NSEventTypeLeftMouseDown;
  const bool scroll = type == NSEventTypeScrollWheel;
  glim::Vec2 local{};
  std::uint32_t id = 0;
  if (down || scroll) {
    id = [self seatAt:p local:&local];
    if (down) captured_ = id;
  } else if (captured_ != 0) {
    id = captured_;
    const Projected& seat = projected_[id];
    local = glim::unprojectPlane(seat.transform, p).value_or(glim::Vec2{-1e6f, -1e6f});
  }
  if (id == 0) return NO;
  const Projected& seat = projected_[id];
  if (scroll) {
    [seat.native scrollWheel:event];
    return YES;
  }
  // The native view converts window points with untransformed geometry,
  // so a point given in that geometry lands where the user aimed.
  const NSPoint inHost =
    NSMakePoint(local.x - seat.local.origin.x, local.y - seat.local.origin.y);
  NSEvent* forwarded = [NSEvent mouseEventWithType:type
                                          location:[seat.host convertPoint:inHost toView:nil]
                                     modifierFlags:event.modifierFlags
                                         timestamp:event.timestamp
                                      windowNumber:event.windowNumber
                                           context:nil
                                       eventNumber:event.eventNumber
                                        clickCount:event.clickCount
                                          pressure:event.pressure];
  if (down) {
    [root.window makeFirstResponder:seat.native];
    [seat.native mouseDown:forwarded];
  } else if (type == NSEventTypeLeftMouseDragged) {
    [seat.native mouseDragged:forwarded];
  } else if (type == NSEventTypeLeftMouseUp) {
    [seat.native mouseUp:forwarded];
    captured_ = 0;
  }
  return YES;
}

@end
