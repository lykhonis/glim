#pragma once

// Core Animation side of a layered seat, shared by the Apple shells: the
// replicator shows the live view again at its first copy, and a mask holds
// each drawing's fade (K89).

#import <QuartzCore/QuartzCore.h>

#include <glim/shell/Slot.h>
#include <glim/shell/SlotLayer.h>

namespace glim::shell {

inline CAGradientLayer* slotFadeMask(Vec2 size, const AlphaRamp& fade, CATransform3D at) {
  CAGradientLayer* g = [CAGradientLayer layer];
  g.anchorPoint = CGPointZero;
  g.bounds = CGRectMake(0, 0, size.x, size.y);
  g.position = CGPointZero;
  g.transform = at;
  const RampLine line = rampLine(fade);
  g.startPoint = CGPointMake(line.start.x, line.start.y);
  g.endPoint = CGPointMake(line.end.x, line.end.y);
  g.colors = @[
    (__bridge_transfer id)CGColorCreateSRGB(0, 0, 0, fade.fromAlpha),
    (__bridge_transfer id)CGColorCreateSRGB(0, 0, 0, fade.toAlpha),
  ];
  return g;
}

// With no copy and no fade there is no mask.
inline void replicateSlot(CAReplicatorLayer* layer, const SlotFrame& frame) {
  const bool copied = !frame.copies.empty();
  layer.instanceCount = copied ? 2 : 1;
  layer.instanceTransform = copied ? toCoreAnimation<CATransform3D>(slotCopyTransform(
                                       frame.local, frame.transform, frame.copies.front()
                                     ))
                                   : CATransform3DIdentity;
  if (!copied && frame.fade.opaque()) {
    layer.mask = nil;
    return;
  }
  CALayer* mask = [CALayer layer];
  mask.frame = CGRectMake(0, 0, frame.local.size.x, frame.local.size.y);
  [mask addSublayer:slotFadeMask(frame.local.size, frame.fade, CATransform3DIdentity)];
  if (copied) {
    const PlacedRect& copy = frame.copies.front();
    [mask addSublayer:slotFadeMask(copy.local.size, copy.fade, layer.instanceTransform)];
  }
  layer.mask = mask;
}

}  // namespace glim::shell
