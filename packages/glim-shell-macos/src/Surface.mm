#include <glim/shell/Surface.h>

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <cmath>

namespace glim::shell {

Surface::Surface() = default;

Surface::~Surface() {
  detach();
  if (layer_) {
    CFRelease(layer_);
    layer_ = nullptr;
  }
}

void Surface::attach(Window& window) {
  detach();
  NSView* view = (__bridge NSView*)window.nativeView();
  CAMetalLayer* layer = [CAMetalLayer layer];
  layer.device = MTLCreateSystemDefaultDevice();
  // sRGB swapchain: shaders output LINEAR, hardware encodes on write
  // and blends against a decoded destination. Working-space hinge:
  // instance colors arrive linearized (premulLinear), sampled bytes
  // linearize in-shader, CPU delinearizes at PNG/readout.
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
  layer.framebufferOnly = NO;
  layer.opaque = YES;
  // Never distort a stale frame: tick paints lag ~1 frame behind a
  // fast drag (several px), and a Resize stretch that large is
  // grotesque. A 1:1 stale frame trails visibly but stays composed;
  // the transaction gate below keeps it glued to the geometry.
  layer.contentsGravity = kCAGravityTopLeft;
  static CGColorSpaceRef srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  layer.colorspace = srgb;
  layer.wantsExtendedDynamicRangeContent = NO;
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
  if (@available(macOS 26.0, *)) {
    layer.preferredDynamicRange = CADynamicRangeStandard;
  }
#endif
  const CGFloat scale = view.window ? view.window.backingScaleFactor : 1.0;
  layer.contentsScale = scale;
  layer.drawableSize = CGSizeMake(
      std::max(1.0, std::round(view.bounds.size.width * scale)),
      std::max(1.0, std::round(view.bounds.size.height * scale))
  );
  if (@available(macOS 10.13, *)) {
    layer.displaySyncEnabled = vsync_;
  }
  view.wantsLayer = YES;
  view.layer = layer;
  view_ = (__bridge void*)view;
  layer_ = (__bridge_retained void*)layer;
}

void Surface::detach() {
  if (view_) {
    NSView* view = (__bridge NSView*)view_;
    if (view.layer == (__bridge CAMetalLayer*)layer_) {
      view.layer = nil;
    }
    view_ = nullptr;
  }
}

void Surface::setVSync(bool enabled) {
  vsync_ = enabled;
  if (layer_) {
    if (@available(macOS 10.13, *)) {
      ((__bridge CAMetalLayer*)layer_).displaySyncEnabled = enabled;
    }
  }
}

void Surface::setOpaque(bool opaque) {
  if (layer_) {
    ((__bridge CAMetalLayer*)layer_).opaque = opaque;
  }
}

gpu::DeviceCreateInfo Surface::deviceCreateInfo() const {
  gpu::DeviceCreateInfo info;
  info.backend = gpu::Backend::Metal;
  info.native[0] = layer_;
  CAMetalLayer* layer = (__bridge CAMetalLayer*)layer_;
  info.native[1] = (__bridge void*)layer.device;
  return info;
}

Vec2 Surface::drawableSize() const {
  if (!layer_) {
    return {};
  }
  const CGSize s = ((__bridge CAMetalLayer*)layer_).drawableSize;
  return {static_cast<float>(s.width), static_cast<float>(s.height)};
}

float Surface::pixelRatio() const {
  if (!layer_) {
    return 1.0f;
  }
  return static_cast<float>(((__bridge CAMetalLayer*)layer_).contentsScale);
}

void* Surface::nativeLayer() const { return layer_; }

}  // namespace glim::shell
