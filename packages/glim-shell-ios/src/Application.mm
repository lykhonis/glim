#include <glim/shell/Application.h>
#include <glim/shell/Event.h>
#include <glim/shell/Window.h>

#include "WindowRegistry.h"

#import <QuartzCore/CADisplayLink.h>
#import <UIKit/UIKit.h>

using glim::shell::Event;
using glim::shell::EventType;
using glim::shell::Window;

@interface GlimDisplayLinkTarget : NSObject
- (void)tick:(CADisplayLink*)link;
@end

@implementation GlimDisplayLinkTarget
- (void)tick:(CADisplayLink*)link {
    (void)link;
    const Event frame(EventType::Frame);
    for (Window* window : glim::shell::detail::shownWindows()) {
        window->dispatch(frame);
    }
}
@end

namespace {

CADisplayLink* gDisplayLink = nil;
GlimDisplayLinkTarget* gDisplayTarget = nil;

void GlimBindWindowsToScene(UIWindowScene* scene) {
    if (!scene) {
        return;
    }
    const auto& shown = glim::shell::detail::shownWindows();
    for (Window* w : shown) {
        UIView* view = (__bridge UIView*)w->nativeView();
        UIWindow* window = view.window;
        if (!window) {
            continue;
        }
        window.windowScene = scene;
        window.frame = scene.coordinateSpace.bounds;
        [window makeKeyAndVisible];
        [window layoutIfNeeded];
    }
}

void GlimEnsureDisplayLink() {
    if (gDisplayLink) {
        return;
    }
    gDisplayTarget = [GlimDisplayLinkTarget new];
    gDisplayLink = [CADisplayLink displayLinkWithTarget:gDisplayTarget selector:@selector(tick:)];
    gDisplayLink.preferredFramesPerSecond = 60;
    [gDisplayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

void GlimPauseDisplayLink(BOOL paused) {
    gDisplayLink.paused = paused;
}

}  // namespace

@interface GlimSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(nonatomic, strong) UIWindow* window;
@end

@implementation GlimSceneDelegate
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session
      options:(UISceneConnectionOptions*)options {
    (void)session;
    (void)options;
    UIWindowScene* ws = [scene isKindOfClass:[UIWindowScene class]] ? (UIWindowScene*)scene : nil;
    GlimBindWindowsToScene(ws);
    const auto& shown = glim::shell::detail::shownWindows();
    if (!shown.empty()) {
        UIView* view = (__bridge UIView*)shown.front()->nativeView();
        self.window = view.window;
    }
    GlimEnsureDisplayLink();
}

- (void)sceneDidBecomeActive:(UIScene*)scene {
    (void)scene;
    GlimPauseDisplayLink(NO);
}

- (void)sceneWillResignActive:(UIScene*)scene {
    (void)scene;
    GlimPauseDisplayLink(YES);
}
@end

@interface GlimAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow* window;
@end

@implementation GlimAppDelegate

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
    (void)application;
    (void)launchOptions;
    UIWindowScene* scene = nil;
    for (UIScene* connected in UIApplication.sharedApplication.connectedScenes) {
        if ([connected isKindOfClass:[UIWindowScene class]]) {
            scene = (UIWindowScene*)connected;
            break;
        }
    }
    GlimBindWindowsToScene(scene);
    const auto& shown = glim::shell::detail::shownWindows();
    if (!shown.empty()) {
        UIView* view = (__bridge UIView*)shown.front()->nativeView();
        self.window = view.window;
        if (!scene) {
            [self.window makeKeyAndVisible];
        }
    }
    GlimEnsureDisplayLink();
    return YES;
}

- (UISceneConfiguration*)application:(UIApplication*)application
configurationForConnectingSceneSession:(UISceneSession*)connectingSceneSession
                             options:(UISceneConnectionOptions*)options {
    (void)application;
    (void)options;
    UISceneConfiguration* cfg =
        [[UISceneConfiguration alloc] initWithName:@"Default" sessionRole:connectingSceneSession.role];
    cfg.delegateClass = [GlimSceneDelegate class];
    cfg.sceneClass = [UIWindowScene class];
    return cfg;
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
    (void)application;
    GlimPauseDisplayLink(YES);
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
    (void)application;
    GlimPauseDisplayLink(NO);
}

@end

namespace glim::shell {

Application::Application() = default;

Application::~Application() = default;

void Application::run() {
    (void)[GlimSceneDelegate class];
    char name[] = "glim";
    char* argv[] = {name, nullptr};
    @autoreleasepool {
        UIApplicationMain(1, argv, nil, NSStringFromClass([GlimAppDelegate class]));
    }
}

}  // namespace glim::shell
