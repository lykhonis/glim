#include <glim/shell/Window.h>

#include "WindowRegistry.h"

#include <algorithm>
#include <cmath>
#include <vector>

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

using glim::shell::Event;
using glim::shell::EventType;
using glim::shell::Key;
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

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
  (void)event;
  return YES;
}

- (void)keyDown:(NSEvent*)event {
  if (event.keyCode == 53) {
    Event e(EventType::KeyDown);
    e.setKey(Key::Escape);
    if (eventCallback_) {
      eventCallback_(e);
    }
    return;
  }
  if (event.keyCode == 36) {
    Event e(EventType::KeyDown);
    e.setKey(Key::Enter);
    if (eventCallback_) {
      eventCallback_(e);
    }
    return;
  }
  [super keyDown:event];
}

- (void)mouseDown:(NSEvent*)event {
  if (!eventCallback_) {
    return;
  }
  const NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
  Event e(EventType::PointerDown);
  e.setPoint(static_cast<float>(p.x), static_cast<float>(p.y));
  e.setButton(glim::shell::PointerButton::Left);
  eventCallback_(e);
}

- (void)mouseDragged:(NSEvent*)event {
  if (!eventCallback_) {
    return;
  }
  const NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
  Event e(EventType::PointerMove);
  e.setPoint(static_cast<float>(p.x), static_cast<float>(p.y));
  e.setButton(glim::shell::PointerButton::Left);
  eventCallback_(e);
}

- (void)mouseUp:(NSEvent*)event {
  if (!eventCallback_) {
    return;
  }
  const NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
  Event e(EventType::PointerUp);
  e.setPoint(static_cast<float>(p.x), static_cast<float>(p.y));
  e.setButton(glim::shell::PointerButton::Left);
  eventCallback_(e);
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

- (void)viewWillStartLiveResize {
  [super viewWillStartLiveResize];
  // The host paints synchronously on WindowResized, i.e. inside this
  // tracking iteration: gating the present on the commit lands it
  // with the same commit carrying this iteration's geometry and seat
  // frames — glued by construction. (Tick-driven paints must NOT use
  // this: outside an iteration the gated present chases a later
  // commit and skews worse.)
  if ([self.layer isKindOfClass:[CAMetalLayer class]]) {
    ((CAMetalLayer*)self.layer).presentsWithTransaction = YES;
  }
}

- (void)viewDidEndLiveResize {
  [super viewDidEndLiveResize];
  if ([self.layer isKindOfClass:[CAMetalLayer class]]) {
    ((CAMetalLayer*)self.layer).presentsWithTransaction = NO;
  }
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
  const NSSize backing = [self convertSizeToBacking:self.bounds.size];
  // Join the in-flight implicit transaction — do NOT begin/commit
  // here. An explicit commit per resize event forces Core Animation
  // to composite a partial cascade mid-drag (siblings still
  // updating), so our layer lands at different times than the
  // native seat views and shimmers against them along the drag
  // axis. Joined properties apply atomically at the natural commit
  // (iteration end), glued to everything else.
  layer.contentsScale = scale;
  // 1:1, never stretched (see Surface::attach): tick paints lag ~1
  // frame behind a fast drag, and a Resize stretch that large is
  // grotesque. A stale frame trails visibly but stays composed.
  layer.contentsGravity = kCAGravityTopLeft;
  // Integral device pixels. Fractional drawable sizes make the
  // compositor resample edges every frame (shimmer), and fractional
  // sizes feed back into window.size() so consecutive paints lay out
  // at ever-so-slightly different sizes (crawl). Quantizing here
  // puts layout on the half-logical grid — identical frames across
  // fractional drag movement, and the host's size read-back stays
  // exactly consistent with this.
  layer.drawableSize = CGSizeMake(
      std::max(1.0, std::round(backing.width)), std::max(1.0, std::round(backing.height))
  );
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
  NSWindow* window = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 640, 380)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  GlimView* view = [[GlimView alloc] initWithFrame:window.contentView.frame];
  view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  view.postsFrameChangedNotifications = YES;
  view.wantsLayer = YES;
  view.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
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
  slots_.forEach([](std::uint32_t, void* view) {
    NSView* child = (__bridge NSView*)view;
    [child removeFromSuperview];
  });
  slots_.clear();
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

void Window::attachToScene(void*) {}

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

void Window::center() { [(__bridge NSWindow*)window_ center]; }

void Window::setEventCallback(EventCallback callback) {
  [(__bridge GlimView*)view_ setEventCallback:std::move(callback)];
}

Vec2 Window::drawableSize() const {
  NSView* view = (__bridge NSView*)view_;
  if ([view.layer isKindOfClass:[CAMetalLayer class]]) {
    const CGSize s = ((CAMetalLayer*)view.layer).drawableSize;
    return {static_cast<float>(s.width), static_cast<float>(s.height)};
  }
  const NSSize backing = [view convertSizeToBacking:view.bounds.size];
  return {static_cast<float>(backing.width), static_cast<float>(backing.height)};
}

float Window::pixelRatio() const {
  NSWindow* window = (__bridge NSWindow*)window_;
  const CGFloat scale = window ? window.backingScaleFactor : 1.0;
  return static_cast<float>(scale);
}

Vec2 Window::size() const {
  const float r = pixelRatio();
  const Vec2 drawable = drawableSize();
  if (r <= 0.f) {
    return drawable;
  }
  return {drawable.x / r, drawable.y / r};
}

Rect Window::safeArea() const { return Rect::fromSize(size()); }

void* Window::nativeView() const { return view_; }

void Window::attachSlot(std::uint32_t id, SlotNative native) {
  NSView* parent = (__bridge NSView*)view_;
  NSView* child = (__bridge NSView*)native.view;
  if (!parent || !child || id == 0) {
    return;
  }
  if (void* prev = slots_.get(id); prev && prev != native.view) {
    [(__bridge NSView*)prev removeFromSuperview];
  }
  [parent addSubview:child];
  slots_.attach(id, native.view);
}

void Window::positionSlot(std::uint32_t id, Rect windowLogical) {
  NSView* child = (__bridge NSView*)slots_.get(id);
  if (!child) {
    return;
  }
  [child setFrame:NSMakeRect(
                      windowLogical.origin.x,
                      windowLogical.origin.y,
                      windowLogical.size.x,
                      windowLogical.size.y
                  )];
}

void Window::detachSlot(std::uint32_t id) {
  NSView* child = (__bridge NSView*)slots_.detach(id);
  [child removeFromSuperview];
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
  CGDataProviderRef provider =
      CGDataProviderCreateWithData(nullptr, buf.data(), buf.size(), nullptr);
  if (!provider) {
    CGColorSpaceRelease(space);
    return;
  }
  CGImageRef image = CGImageCreate(
      static_cast<size_t>(s->width),
      static_cast<size_t>(s->height),
      8,
      32,
      static_cast<size_t>(s->width) * 4,
      space,
      static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast),
      provider,
      nullptr,
      false,
      kCGRenderingIntentAbsoluteColorimetric
  );
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
  view.layer.contentsGravity = kCAGravityTopLeft;
  view.layer.contents = (__bridge id)image;
  CGImageRelease(image);
  s->back ^= 1;
}
#endif

void Window::dispatch(const Event& event) { [(__bridge GlimView*)view_ dispatchEvent:event]; }

}  // namespace glim::shell
