#include <glim/shell/Window.h>

#include "WindowRegistry.h"

#include <vector>

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

using glim::shell::Event;
using glim::shell::EventType;
using glim::shell::Window;

@interface GlimView : NSView <NSWindowDelegate>
- (void)setEventCallback:(Window::EventCallback)callback;
- (void)dispatchEvent:(const Event&)event;
@end

@implementation GlimView {
    Window::EventCallback eventCallback_;
}

- (void)setEventCallback:(Window::EventCallback)callback {
    eventCallback_ = std::move(callback);
}

- (void)dispatchEvent:(const Event&)event {
    if (eventCallback_) {
        eventCallback_(event);
    }
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)isOpaque {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    self.postsFrameChangedNotifications = YES;
    [self syncMetalDrawableSize];
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    [self notifyResized];
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    [self notifyResized];
}

- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    if (eventCallback_) {
        eventCallback_(Event(EventType::WindowClosed));
    }
}

- (void)syncMetalDrawableSize {
    if (![self.layer isKindOfClass:[CAMetalLayer class]]) {
        return;
    }
    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    const CGFloat scale = self.window ? self.window.backingScaleFactor : 1.0;
    layer.contentsScale = scale;
    layer.drawableSize = CGSizeMake(self.bounds.size.width * scale, self.bounds.size.height * scale);
}

- (void)notifyResized {
    [self syncMetalDrawableSize];
    if (!eventCallback_) {
        return;
    }
    const NSSize size = self.bounds.size;
    Event resized(EventType::WindowResized);
    resized.setSize(static_cast<int>(size.width), static_cast<int>(size.height));
    eventCallback_(resized);
}

@end

#if GLIM_SOFTWARE
namespace {
struct SoftwarePresent {
    std::vector<std::uint8_t> buffers[2];
    int width = 0;
    int height = 0;
    int back = 0;
};
}  // namespace
#endif

namespace glim::shell {

Window::Window() {
    NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 380)
                                                   styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                             NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    GlimView* view = [[GlimView alloc] initWithFrame:window.contentView.frame];
    view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    view.postsFrameChangedNotifications = YES;
    view.wantsLayer = YES;
    window.delegate = view;
    window.releasedWhenClosed = NO;
    window.colorSpace = [NSColorSpace sRGBColorSpace];
    [window.contentView addSubview:view];
    [window makeFirstResponder:view];
    window_ = (__bridge_retained void*)window;
    view_ = (__bridge_retained void*)view;
}

Window::~Window() {
    detail::unregisterShownWindow(this);
#if GLIM_SOFTWARE
    if (software_) {
        if (view_) {
            ((__bridge NSView*)view_).layer.contents = nil;
        }
        auto* s = static_cast<SoftwarePresent*>(software_);
        delete s;
        software_ = nullptr;
    }
#endif
    if (view_) {
        CFRelease(view_);
        view_ = nullptr;
    }
    if (window_) {
        CFRelease(window_);
        window_ = nullptr;
    }
}

void Window::show() {
    NSWindow* window = (__bridge NSWindow*)window_;
    [window makeKeyAndOrderFront:nil];
    if (!shown_) {
        shown_ = true;
        detail::registerShownWindow(this);
    }
}

void Window::hide() {
    NSWindow* window = (__bridge NSWindow*)window_;
    [window orderOut:nil];
    shown_ = false;
    detail::unregisterShownWindow(this);
}

void Window::setSize(int width, int height) {
    NSWindow* window = (__bridge NSWindow*)window_;
    [window setContentSize:NSMakeSize(width, height)];
}

void Window::setTitle(const std::string& title) {
    NSWindow* window = (__bridge NSWindow*)window_;
    window.title = [NSString stringWithUTF8String:title.c_str()];
}

void Window::center() {
    [(__bridge NSWindow*)window_ center];
}

void Window::setEventCallback(EventCallback callback) {
    [(__bridge GlimView*)view_ setEventCallback:std::move(callback)];
}

Vec2 Window::size() const {
    const NSSize s = ((__bridge GlimView*)view_).bounds.size;
    return {static_cast<float>(s.width), static_cast<float>(s.height)};
}

float Window::pixelRatio() const {
    NSWindow* window = (__bridge NSWindow*)window_;
    const CGFloat scale = window ? window.backingScaleFactor : 1.0;
    return static_cast<float>(scale);
}

Vec2 Window::drawableSize() const {
    const Vec2 logical = size();
    const float r = pixelRatio();
    return {logical.x * r, logical.y * r};
}

void* Window::nativeView() const {
    return view_;
}

#if GLIM_SOFTWARE
std::uint8_t* Window::mapSoftware(int width, int height) {
    if (width <= 0 || height <= 0) {
        return nullptr;
    }
    auto* s = static_cast<SoftwarePresent*>(software_);
    if (!s) {
        s = new SoftwarePresent();
        software_ = s;
    }
    if (width != s->width || height != s->height) {
        if (view_) {
            ((__bridge NSView*)view_).layer.contents = nil;
        }
        const std::size_t n = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
        s->buffers[0].assign(n, 0);
        s->buffers[1].assign(n, 0);
        s->width = width;
        s->height = height;
        s->back = 0;
    }
    return s->buffers[s->back].data();
}

void Window::presentSoftware() {
    if (!software_ || !view_) {
        return;
    }
    auto* s = static_cast<SoftwarePresent*>(software_);
    const std::vector<std::uint8_t>& buf = s->buffers[s->back];
    if (buf.empty()) {
        return;
    }
    NSView* view = (__bridge NSView*)view_;
    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGDataProviderRef provider = CGDataProviderCreateWithData(nullptr, buf.data(), buf.size(), nullptr);
    if (!provider) {
        CGColorSpaceRelease(space);
        return;
    }
    CGImageRef image =
        CGImageCreate(static_cast<size_t>(s->width), static_cast<size_t>(s->height), 8, 32,
                      static_cast<size_t>(s->width) * 4, space,
                      static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast), provider, nullptr, false,
                      kCGRenderingIntentAbsoluteColorimetric);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(space);
    if (!image) {
        return;
    }
    view.wantsLayer = YES;
    view.layer.opaque = YES;
    if (view.window) {
        view.layer.contentsScale = view.window.backingScaleFactor;
    }
    view.layer.contentsGravity = kCAGravityResize;
    view.layer.contents = (__bridge id)image;
    CGImageRelease(image);
    s->back ^= 1;
}
#endif

void Window::dispatch(const Event& event) {
    [(__bridge GlimView*)view_ dispatchEvent:event];
}

}  // namespace glim::shell
