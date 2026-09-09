#include <glim/shell/Window.h>

#include "WindowRegistry.h"

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

- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    [self syncMetalDrawableSize];
    if (!eventCallback_) {
        return;
    }
    const NSSize size = self.bounds.size;
    Event e(EventType::WindowResized);
    e.setSize(static_cast<int>(size.width), static_cast<int>(size.height));
    eventCallback_(e);
}

@end

namespace glim::shell {

Window::Window() {
    NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 380)
                                                   styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                             NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    GlimView* view = [[GlimView alloc] initWithFrame:window.contentView.frame];
    view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    view.wantsLayer = YES;
    window.delegate = view;
    window.releasedWhenClosed = NO;
    [window.contentView addSubview:view];
    [window makeFirstResponder:view];
    window_ = (__bridge_retained void*)window;
    view_ = (__bridge_retained void*)view;
}

Window::~Window() {
    detail::unregisterShownWindow(this);
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

void Window::dispatch(const Event& event) {
    [(__bridge GlimView*)view_ dispatchEvent:event];
}

}  // namespace glim::shell
