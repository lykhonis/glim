#include <glim/shell/Window.h>

#include "WindowRegistry.h"

#import <QuartzCore/CAMetalLayer.h>
#import <UIKit/UIKit.h>

#include <TargetConditionals.h>
#include <algorithm>

using glim::shell::Event;
using glim::shell::EventType;
using glim::shell::Key;
using glim::shell::PointerButton;
using glim::shell::Window;

@interface GlimView : UIView
- (void)setEventCallback:(Window::EventCallback)callback;
- (void)dispatchEvent:(const Event&)event;
- (void)syncMetalDrawableSize;
- (void)notifyResizedIfNeeded;
- (glim::Rect)safeAreaRect;
@end

@implementation GlimView {
    Window::EventCallback eventCallback_;
    CGSize lastLogicalSize_;
}

+ (Class)layerClass {
    return [CAMetalLayer class];
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.opaque = YES;
#if !TARGET_OS_TV
        self.multipleTouchEnabled = YES;
#endif
        lastLogicalSize_ = CGSizeMake(-1, -1);
        const CGFloat scale = self.traitCollection.displayScale > 0 ? self.traitCollection.displayScale : 1.0;
        self.contentScaleFactor = scale;
    }
    return self;
}

- (void)setEventCallback:(Window::EventCallback)callback {
    eventCallback_ = std::move(callback);
}

- (void)dispatchEvent:(const Event&)event {
    if (eventCallback_) {
        eventCallback_(event);
    }
}

- (void)layoutSubviews {
    [super layoutSubviews];
    [self notifyResizedIfNeeded];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previous {
    [super traitCollectionDidChange:previous];
    [self notifyResizedIfNeeded];
}

- (void)syncMetalDrawableSize {
    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    const CGFloat scale = self.traitCollection.displayScale > 0 ? self.traitCollection.displayScale : 1.0;
    self.contentScaleFactor = scale;
    layer.contentsScale = scale;
    layer.drawableSize = CGSizeMake(self.bounds.size.width * scale, self.bounds.size.height * scale);
}

- (void)notifyResizedIfNeeded {
    [self syncMetalDrawableSize];
    const CGSize size = self.bounds.size;
    if (CGSizeEqualToSize(size, lastLogicalSize_)) {
        return;
    }
    lastLogicalSize_ = size;
    if (!eventCallback_) {
        return;
    }
    Event resized(EventType::WindowResized);
    resized.setSize(static_cast<int>(size.width), static_cast<int>(size.height));
    eventCallback_(resized);
}

- (glim::Rect)safeAreaRect {
    const UIEdgeInsets insets = self.safeAreaInsets;
    const CGSize size = self.bounds.size;
    const float x = static_cast<float>(insets.left);
    const float y = static_cast<float>(insets.top);
    const float w = static_cast<float>(std::max<CGFloat>(0, size.width - insets.left - insets.right));
    const float h = static_cast<float>(std::max<CGFloat>(0, size.height - insets.top - insets.bottom));
    return glim::Rect{{x, y}, {w, h}};
}

#if !TARGET_OS_TV
- (void)emitPointer:(EventType)type touch:(UITouch*)touch {
    if (!eventCallback_) {
        return;
    }
    const CGPoint p = [touch locationInView:self];
    Event e(type);
    e.setPoint(static_cast<float>(p.x), static_cast<float>(p.y));
    e.setButton(PointerButton::Left);
    e.setPointerId(static_cast<int>(touch.hash & 0x7fffffff));
    eventCallback_(e);
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    for (UITouch* touch in touches) {
        [self emitPointer:EventType::PointerDown touch:touch];
    }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    for (UITouch* touch in touches) {
        [self emitPointer:EventType::PointerMove touch:touch];
    }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    for (UITouch* touch in touches) {
        [self emitPointer:EventType::PointerUp touch:touch];
    }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    for (UITouch* touch in touches) {
        [self emitPointer:EventType::PointerUp touch:touch];
    }
}
#endif

@end

@interface GlimViewController : UIViewController
@end

@implementation GlimViewController

- (void)loadView {
    CGRect frame = CGRectMake(0, 0, 320, 480);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (UIScreen.mainScreen) {
        frame = UIScreen.mainScreen.bounds;
    }
#pragma clang diagnostic pop
    GlimView* view = [[GlimView alloc] initWithFrame:frame];
    view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.view = view;
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    [(GlimView*)self.view notifyResizedIfNeeded];
}

- (void)viewSafeAreaInsetsDidChange {
    [super viewSafeAreaInsetsDidChange];
    [(GlimView*)self.view notifyResizedIfNeeded];
}

#if TARGET_OS_TV
- (void)pressesBegan:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event {
    GlimView* view = (GlimView*)self.view;
    bool handled = false;
    for (UIPress* press in presses) {
        Key key = Key::Unknown;
        switch (press.type) {
            case UIPressTypeUpArrow:
                key = Key::DpadUp;
                break;
            case UIPressTypeDownArrow:
                key = Key::DpadDown;
                break;
            case UIPressTypeLeftArrow:
                key = Key::DpadLeft;
                break;
            case UIPressTypeRightArrow:
                key = Key::DpadRight;
                break;
            case UIPressTypeSelect:
                key = Key::DpadCenter;
                break;
            case UIPressTypeMenu:
                key = Key::Back;
                break;
            default:
                break;
        }
        if (key != Key::Unknown) {
            Event e(EventType::KeyDown);
            e.setKey(key);
            [view dispatchEvent:e];
            handled = true;
        }
    }
    if (!handled) {
        [super pressesBegan:presses withEvent:event];
    }
}

- (void)pressesEnded:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event {
    GlimView* view = (GlimView*)self.view;
    bool handled = false;
    for (UIPress* press in presses) {
        Key key = Key::Unknown;
        switch (press.type) {
            case UIPressTypeUpArrow:
                key = Key::DpadUp;
                break;
            case UIPressTypeDownArrow:
                key = Key::DpadDown;
                break;
            case UIPressTypeLeftArrow:
                key = Key::DpadLeft;
                break;
            case UIPressTypeRightArrow:
                key = Key::DpadRight;
                break;
            case UIPressTypeSelect:
                key = Key::DpadCenter;
                break;
            case UIPressTypeMenu:
                key = Key::Back;
                break;
            default:
                break;
        }
        if (key != Key::Unknown) {
            Event e(EventType::KeyUp);
            e.setKey(key);
            [view dispatchEvent:e];
            handled = true;
        }
    }
    if (!handled) {
        [super pressesEnded:presses withEvent:event];
    }
}
#endif

@end

namespace glim::shell {

Window::Window() {
    @autoreleasepool {
        CGRect frame = CGRectMake(0, 0, 320, 480);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        if (UIScreen.mainScreen) {
            frame = UIScreen.mainScreen.bounds;
        }
#pragma clang diagnostic pop
        UIWindow* window = [[UIWindow alloc] initWithFrame:frame];
        GlimViewController* vc = [[GlimViewController alloc] init];
        window.rootViewController = vc;
        window.backgroundColor = [UIColor blackColor];
        window_ = (__bridge_retained void*)window;
        view_ = (__bridge_retained void*)vc.view;
    }
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
    UIWindow* window = (__bridge UIWindow*)window_;
    [window makeKeyAndVisible];
    if (!shown_) {
        shown_ = true;
        detail::registerShownWindow(this);
    }
}

void Window::hide() {
    UIWindow* window = (__bridge UIWindow*)window_;
    window.hidden = YES;
    shown_ = false;
    detail::unregisterShownWindow(this);
}

void Window::setSize(int, int) {
    // UIWindow fills the screen / scene on iOS and tvOS.
}

void Window::setTitle(const std::string& title) {
    UIWindow* window = (__bridge UIWindow*)window_;
    window.rootViewController.title = [NSString stringWithUTF8String:title.c_str()];
}

void Window::center() {}

void Window::setEventCallback(EventCallback callback) {
    [(__bridge GlimView*)view_ setEventCallback:std::move(callback)];
}

Vec2 Window::size() const {
    const CGSize s = ((__bridge GlimView*)view_).bounds.size;
    return {static_cast<float>(s.width), static_cast<float>(s.height)};
}

float Window::pixelRatio() const {
    GlimView* view = (__bridge GlimView*)view_;
    const CGFloat scale = view.traitCollection.displayScale > 0 ? view.traitCollection.displayScale : 1.0;
    return static_cast<float>(scale);
}

Vec2 Window::drawableSize() const {
    const Vec2 logical = size();
    const float r = pixelRatio();
    return {logical.x * r, logical.y * r};
}

Rect Window::safeArea() const {
    return [(__bridge GlimView*)view_ safeAreaRect];
}

void* Window::nativeView() const {
    return view_;
}

void Window::dispatch(const Event& event) {
    if (event.type() == EventType::Frame || event.type() == EventType::WindowResized) {
        [(__bridge GlimView*)view_ syncMetalDrawableSize];
    }
    [(__bridge GlimView*)view_ dispatchEvent:event];
}

}  // namespace glim::shell
