#include <glim/shell/Surface.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <UIKit/UIKit.h>

#include <TargetConditionals.h>

namespace glim::shell {

Surface::Surface() = default;

Surface::~Surface() {
    detach();
}

void Surface::attach(Window& window) {
    detach();
    UIView* view = (__bridge UIView*)window.nativeView();
    CAMetalLayer* layer = (CAMetalLayer*)view.layer;
    if (![layer isKindOfClass:[CAMetalLayer class]]) {
        return;
    }
    layer.device = MTLCreateSystemDefaultDevice();
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;
    layer.opaque = YES;
    layer.contentsGravity = kCAGravityResize;
    layer.presentsWithTransaction = NO;
    static CGColorSpaceRef srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    layer.colorspace = srgb;
#if !TARGET_OS_TV
    layer.wantsExtendedDynamicRangeContent = NO;
#endif
    const CGFloat scale = view.traitCollection.displayScale > 0 ? view.traitCollection.displayScale : 1.0;
    view.contentScaleFactor = scale;
    layer.contentsScale = scale;
    layer.drawableSize = CGSizeMake(view.bounds.size.width * scale, view.bounds.size.height * scale);
    view_ = (__bridge void*)view;
    layer_ = (__bridge void*)layer;
}

void Surface::detach() {
    view_ = nullptr;
    layer_ = nullptr;
}

void Surface::setVSync(bool enabled) {
    vsync_ = enabled;
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
    info.native[1] = layer ? (__bridge void*)layer.device : nullptr;
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

void* Surface::nativeLayer() const {
    return layer_;
}

}  // namespace glim::shell
