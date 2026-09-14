#include <glim/shell/Window.h>

#include "WindowRegistry.h"

#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>
#import <UIKit/UIKit.h>

#include <TargetConditionals.h>
#include <algorithm>
#include <cmath>

using glim::shell::Event;
using glim::shell::EventType;
using glim::shell::Key;
using glim::shell::PointerButton;
using glim::shell::Window;

namespace {

UIWindowScene* glimSceneForView(UIView* view) {
    if (view.window.windowScene) {
        return view.window.windowScene;
    }
    for (UIScene* connected in UIApplication.sharedApplication.connectedScenes) {
        if ([connected isKindOfClass:[UIWindowScene class]]) {
            return (UIWindowScene*)connected;
        }
    }
    return nil;
}

#if !TARGET_OS_TV
UIInterfaceOrientation glimSceneOrientation(UIWindowScene* scene) {
    if (!scene) {
        return UIInterfaceOrientationUnknown;
    }
    UIInterfaceOrientation orientation = UIInterfaceOrientationUnknown;
    if (@available(iOS 16.0, *)) {
        orientation = scene.effectiveGeometry.interfaceOrientation;
    }
    if (orientation == UIInterfaceOrientationUnknown) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        orientation = scene.interfaceOrientation;
#pragma clang diagnostic pop
    }
    return orientation;
}

BOOL glimWantLandscape(UIWindowScene* scene) {
    NSString* forced = NSProcessInfo.processInfo.environment[@"GLIM_SIM_ORIENTATION"];
    if (forced.length > 0) {
        return [forced.lowercaseString hasPrefix:@"land"];
    }
    const UIDeviceOrientation device = UIDevice.currentDevice.orientation;
    if (UIDeviceOrientationIsLandscape(device)) {
        return YES;
    }
    if (UIDeviceOrientationIsPortrait(device)) {
        return NO;
    }
    const UIInterfaceOrientation orientation = glimSceneOrientation(scene);
    if (UIInterfaceOrientationIsLandscape(orientation)) {
        return YES;
    }
    if (UIInterfaceOrientationIsPortrait(orientation)) {
        return NO;
    }
    return NO;
}
#endif

CGSize glimIntendedPointSize(UIWindowScene* scene, UIView* view) {
    UIScreen* screen = scene.screen;
    if (!screen && view) {
        screen = view.window.screen;
    }
    if (!screen) {
        screen = UIScreen.mainScreen;
    }
    const CGFloat scale = screen.scale > 0 ? screen.scale : 1.0;
    CGSize size = CGSizeMake(screen.nativeBounds.size.width / scale, screen.nativeBounds.size.height / scale);
    if (size.width < 1.0 || size.height < 1.0) {
        size = screen.bounds.size;
    }
#if !TARGET_OS_TV
    const BOOL landscape = glimWantLandscape(scene);
    if (landscape && size.width < size.height) {
        size = CGSizeMake(size.height, size.width);
    } else if (!landscape && size.width > size.height) {
        size = CGSizeMake(size.height, size.width);
    }
#endif
    if (size.width < 1.0) {
        size.width = 1.0;
    }
    if (size.height < 1.0) {
        size.height = 1.0;
    }
    return size;
}

#if !TARGET_OS_TV
void glimRequestSceneGeometry(UIWindowScene* scene) {
    if (!scene) {
        return;
    }
    if (@available(iOS 16.0, *)) {
        UIInterfaceOrientationMask mask = UIInterfaceOrientationMaskAllButUpsideDown;
        if (glimWantLandscape(scene)) {
            mask = UIInterfaceOrientationMaskLandscape;
        }
        UIWindowSceneGeometryPreferencesIOS* prefs =
            [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:mask];
        [scene requestGeometryUpdateWithPreferences:prefs errorHandler:^(NSError*){
        }];
    }
}
#endif

}  // namespace

@interface GlimView : UIView
- (void)setEventCallback:(Window::EventCallback)callback;
- (void)dispatchEvent:(const Event&)event;
- (void)syncMetalDrawableSize;
- (void)notifyResizedIfNeeded;
- (glim::Rect)safeAreaRect;
@end

@implementation GlimView {
    Window::EventCallback eventCallback_;
    CGSize lastBackingSize_;
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
        lastBackingSize_ = CGSizeMake(-1, -1);
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

- (void)didMoveToWindow {
    [super didMoveToWindow];
    lastBackingSize_ = CGSizeMake(-1, -1);
    [self notifyResizedIfNeeded];
}

- (void)layoutSubviews {
    [super layoutSubviews];
    [self notifyResizedIfNeeded];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previous {
    [super traitCollectionDidChange:previous];
    [self notifyResizedIfNeeded];
}

- (CGSize)backingPixelSize {
    const CGFloat scale = self.traitCollection.displayScale > 0 ? self.traitCollection.displayScale : 1.0;
    const CGSize points = glimIntendedPointSize(glimSceneForView(self), self);
    return CGSizeMake(std::max(1.0, std::round(points.width * scale)),
                      std::max(1.0, std::round(points.height * scale)));
}

- (void)syncMetalDrawableSize {
    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    const CGFloat scale = self.traitCollection.displayScale > 0 ? self.traitCollection.displayScale : 1.0;
    self.contentScaleFactor = scale;
    const CGSize backing = [self backingPixelSize];
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    layer.contentsScale = scale;
    layer.contentsGravity = kCAGravityResize;
    layer.drawableSize = backing;
    [CATransaction commit];
}

- (void)notifyResizedIfNeeded {
    [self syncMetalDrawableSize];
    const CGSize backing = [self backingPixelSize];
    if (CGSizeEqualToSize(backing, lastBackingSize_)) {
        return;
    }
    lastBackingSize_ = backing;
    if (!eventCallback_) {
        return;
    }
    const CGFloat scale = self.contentScaleFactor > 0 ? self.contentScaleFactor : 1.0;
    Event resized(EventType::WindowResized);
    resized.setSize(static_cast<int>(std::lround(backing.width / scale)),
                    static_cast<int>(std::lround(backing.height / scale)));
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
    [super touchesBegan:touches withEvent:event];
    for (UITouch* touch in touches) {
        [self emitPointer:EventType::PointerDown touch:touch];
    }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [super touchesMoved:touches withEvent:event];
    for (UITouch* touch in touches) {
        [self emitPointer:EventType::PointerMove touch:touch];
    }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [super touchesEnded:touches withEvent:event];
    for (UITouch* touch in touches) {
        [self emitPointer:EventType::PointerUp touch:touch];
    }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [super touchesCancelled:touches withEvent:event];
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
    GlimView* view = [[GlimView alloc] initWithFrame:CGRectZero];
    view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.view = view;
}

- (BOOL)shouldAutorotate {
    return YES;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
#if TARGET_OS_TV
    return UIInterfaceOrientationMaskAll;
#else
    return UIInterfaceOrientationMaskAllButUpsideDown;
#endif
}

#if !TARGET_OS_TV
- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
    UIWindowScene* scene = glimSceneForView(self.view);
    if (glimWantLandscape(scene)) {
        const UIInterfaceOrientation o = glimSceneOrientation(scene);
        if (o == UIInterfaceOrientationLandscapeLeft || o == UIInterfaceOrientationLandscapeRight) {
            return o;
        }
        return UIInterfaceOrientationLandscapeRight;
    }
    return UIInterfaceOrientationPortrait;
}
#endif

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    [(GlimView*)self.view notifyResizedIfNeeded];
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    [(GlimView*)self.view notifyResizedIfNeeded];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    GlimView* view = (GlimView*)self.view;
    CAMetalLayer* layer = (CAMetalLayer*)view.layer;
    if (![layer isKindOfClass:[CAMetalLayer class]]) {
        return;
    }
    layer.presentsWithTransaction = YES;
    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext>) {
        [view notifyResizedIfNeeded];
    } completion:^(id<UIViewControllerTransitionCoordinatorContext>) {
        layer.presentsWithTransaction = NO;
        [view notifyResizedIfNeeded];
    }];
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
namespace {

void glimSyncWindowToScene(UIWindow* window, GlimView* view, UIWindowScene* scene) {
#if !TARGET_OS_TV
    glimRequestSceneGeometry(scene);
#else
    (void)scene;
#endif
    [window layoutIfNeeded];
    [view notifyResizedIfNeeded];
}

}  // namespace

Window::Window() {
    @autoreleasepool {
        GlimViewController* vc = [[GlimViewController alloc] init];
        [vc loadViewIfNeeded];
        controller_ = (__bridge_retained void*)vc;
        view_ = (__bridge_retained void*)vc.view;
    }
}

Window::~Window() {
    detail::unregisterShownWindow(this);
    slots_.forEach([](std::uint32_t, void* view) {
        UIView* child = (__bridge UIView*)view;
        [child removeFromSuperview];
    });
    slots_.clear();
    if (view_) {
        CFRelease(view_);
        view_ = nullptr;
    }
    if (window_) {
        CFRelease(window_);
        window_ = nullptr;
    }
    if (controller_) {
        CFRelease(controller_);
        controller_ = nullptr;
    }
}

static UIWindowScene* glimActiveWindowScene() {
    UIApplication* app = UIApplication.sharedApplication;
    if (!app) {
        return nil;
    }
    UIWindowScene* fallback = nil;
    for (UIScene* connected in app.connectedScenes) {
        if (![connected isKindOfClass:[UIWindowScene class]]) {
            continue;
        }
        UIWindowScene* scene = (UIWindowScene*)connected;
        if (scene.activationState == UISceneActivationStateForegroundActive) {
            return scene;
        }
        if (!fallback) {
            fallback = scene;
        }
    }
    return fallback;
}

void Window::attachToScene(void* scenePtr) {
    UIWindowScene* scene = scenePtr ? (__bridge UIWindowScene*)scenePtr : glimActiveWindowScene();
    if (!scene) {
        return;
    }
    GlimView* view = view_ ? (__bridge GlimView*)view_ : nil;
    UIViewController* vc = controller_ ? (__bridge UIViewController*)controller_ : nil;
    if (!view || !vc) {
        return;
    }
    UIWindow* window = window_ ? (__bridge UIWindow*)window_ : nil;
    if (!window) {
        if (scene.windows.count > 0) {
            window = scene.windows.firstObject;
        } else {
            window = [[UIWindow alloc] initWithWindowScene:scene];
        }
        window.backgroundColor = [UIColor blackColor];
        window.rootViewController = vc;
        window_ = (__bridge_retained void*)window;
    } else if (window.windowScene != scene) {
        window.windowScene = scene;
    }
    if (!window.isKeyWindow) {
        [window makeKeyAndVisible];
    }
    glimSyncWindowToScene(window, view, scene);
}

void Window::show() {
    attachToScene(nullptr);
    if (!shown_) {
        shown_ = true;
        detail::registerShownWindow(this);
    }
}

void Window::hide() {
    if (window_) {
        ((__bridge UIWindow*)window_).hidden = YES;
    }
    shown_ = false;
    detail::unregisterShownWindow(this);
}

void Window::setSize(int, int) {
    // UIWindow fills the screen / scene on iOS and tvOS.
}

void Window::setTitle(const std::string& title) {
    UIViewController* vc = controller_ ? (__bridge UIViewController*)controller_ : nil;
    if (vc) {
        vc.title = [NSString stringWithUTF8String:title.c_str()];
    }
}

void Window::center() {}

void Window::setEventCallback(EventCallback callback) {
    [(__bridge GlimView*)view_ setEventCallback:std::move(callback)];
}

Vec2 Window::drawableSize() const {
    UIView* view = (__bridge UIView*)view_;
    if ([view.layer isKindOfClass:[CAMetalLayer class]]) {
        const CGSize s = ((CAMetalLayer*)view.layer).drawableSize;
        return {static_cast<float>(s.width), static_cast<float>(s.height)};
    }
    const CGFloat scale = view.traitCollection.displayScale > 0 ? view.traitCollection.displayScale : 1.0;
    return {static_cast<float>(std::max(1.0, std::round(view.bounds.size.width * scale))),
            static_cast<float>(std::max(1.0, std::round(view.bounds.size.height * scale)))};
}

float Window::pixelRatio() const {
    GlimView* view = (__bridge GlimView*)view_;
    const CGFloat scale = view.traitCollection.displayScale > 0 ? view.traitCollection.displayScale : 1.0;
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

Rect Window::safeArea() const {
    return [(__bridge GlimView*)view_ safeAreaRect];
}

void* Window::nativeView() const {
    return view_;
}

void Window::attachSlot(std::uint32_t id, SlotNative native) {
    UIView* parent = (__bridge UIView*)view_;
    UIView* child = (__bridge UIView*)native.view;
    if (!parent || !child || id == 0) {
        return;
    }
    if (void* prev = slots_.get(id); prev && prev != native.view) {
        [(__bridge UIView*)prev removeFromSuperview];
    }
    [parent addSubview:child];
    slots_.attach(id, native.view);
}

void Window::positionSlot(std::uint32_t id, Rect windowLogical) {
    UIView* child = (__bridge UIView*)slots_.get(id);
    if (!child) {
        return;
    }
    child.frame = CGRectMake(windowLogical.origin.x, windowLogical.origin.y, windowLogical.size.x,
                             windowLogical.size.y);
}

void Window::detachSlot(std::uint32_t id) {
    UIView* child = (__bridge UIView*)slots_.detach(id);
    [child removeFromSuperview];
}

void Window::dispatch(const Event& event) {
    GlimView* view = (__bridge GlimView*)view_;
    if (event.type() == EventType::Frame || event.type() == EventType::WindowResized) {
        [view syncMetalDrawableSize];
    }
    [view dispatchEvent:event];
}

}  // namespace glim::shell
