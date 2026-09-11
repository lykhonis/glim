#include <glim/shell/Application.h>
#include <glim/shell/Event.h>
#include <glim/shell/RunLoop.h>
#include <glim/shell/Window.h>

#include "WindowRegistry.h"

#import <AppKit/AppKit.h>
#import <CoreVideo/CoreVideo.h>
#import <QuartzCore/CADisplayLink.h>

#include <atomic>

using glim::shell::Event;
using glim::shell::EventType;
using glim::shell::Window;

namespace {

id gDisplayLink = nil;
CVDisplayLinkRef gCVLink = nullptr;
std::atomic_bool gCVFramePosted{false};

void GlimDispatchFrame() {
    const Event frame(EventType::Frame);
    for (Window* window : glim::shell::detail::shownWindows()) {
        window->dispatch(frame);
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
CVReturn GlimCVLinkCallback(CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags,
                            CVOptionFlags*, void*) {
    bool expected = false;
    if (!gCVFramePosted.compare_exchange_strong(expected, true)) {
        return kCVReturnSuccess;
    }
    dispatch_async(dispatch_get_main_queue(), ^{
        gCVFramePosted.store(false);
        GlimDispatchFrame();
    });
    return kCVReturnSuccess;
}
#pragma clang diagnostic pop

}  // namespace

@interface GlimDisplayLinkTarget : NSObject
- (void)tick:(id)link;
@end

@implementation GlimDisplayLinkTarget
- (void)tick:(id)link {
    (void)link;
    GlimDispatchFrame();
}
@end

namespace {

GlimDisplayLinkTarget* gDisplayTarget = nil;

void GlimEnsureDisplayLink() {
    if (gDisplayLink || gCVLink) {
        return;
    }
    if (@available(macOS 14.0, *)) {
        gDisplayTarget = [GlimDisplayLinkTarget new];
        NSView* view = nil;
        for (Window* w : glim::shell::detail::shownWindows()) {
            view = (__bridge NSView*)w->nativeView();
            if (view) {
                break;
            }
        }
        if (view) {
            gDisplayLink = [view displayLinkWithTarget:gDisplayTarget selector:@selector(tick:)];
        } else if (NSScreen.mainScreen) {
            gDisplayLink = [NSScreen.mainScreen displayLinkWithTarget:gDisplayTarget selector:@selector(tick:)];
        }
        if (gDisplayLink) {
            CADisplayLink* link = gDisplayLink;
            link.preferredFrameRateRange = CAFrameRateRangeDefault;
            [link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
            return;
        }
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (CVDisplayLinkCreateWithActiveCGDisplays(&gCVLink) != kCVReturnSuccess) {
        gCVLink = nullptr;
        return;
    }
    CVDisplayLinkSetOutputCallback(gCVLink, &GlimCVLinkCallback, nullptr);
    CVDisplayLinkStart(gCVLink);
#pragma clang diagnostic pop
}

void GlimSetDisplayLinkPaused(BOOL paused) {
    if (@available(macOS 14.0, *)) {
        CADisplayLink* link = gDisplayLink;
        link.paused = paused;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (gCVLink) {
        if (paused) {
            CVDisplayLinkStop(gCVLink);
        } else {
            CVDisplayLinkStart(gCVLink);
        }
    }
#pragma clang diagnostic pop
}

void GlimStopDisplayLink() {
    if (@available(macOS 14.0, *)) {
        CADisplayLink* link = gDisplayLink;
        [link invalidate];
        gDisplayLink = nil;
        gDisplayTarget = nil;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (gCVLink) {
        CVDisplayLinkStop(gCVLink);
        CVDisplayLinkRelease(gCVLink);
        gCVLink = nullptr;
    }
#pragma clang diagnostic pop
}

}  // namespace

@interface GlimApplicationDelegate : NSObject <NSApplicationDelegate>
@end

@implementation GlimApplicationDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    [NSApp activateIgnoringOtherApps:YES];
}

- (void)applicationDidBecomeActive:(NSNotification*)notification {
    (void)notification;
    GlimSetDisplayLinkPaused(NO);
}

- (void)applicationDidResignActive:(NSNotification*)notification {
    (void)notification;
    GlimSetDisplayLinkPaused(YES);
}

- (void)applicationDidUnhide:(NSNotification*)notification {
    (void)notification;
    GlimSetDisplayLinkPaused(NO);
}

- (void)applicationDidHide:(NSNotification*)notification {
    (void)notification;
    GlimSetDisplayLinkPaused(YES);
}
@end

namespace glim::shell {
namespace {

NSMenu* makeMenu() {
    NSMenu* menubar = [NSMenu new];
    NSMenuItem* appItem = [NSMenuItem new];
    [menubar addItem:appItem];
    NSMenu* appMenu = [NSMenu new];
    NSMenuItem* quit = [[NSMenuItem alloc] initWithTitle:@"Quit"
                                                  action:@selector(terminate:)
                                           keyEquivalent:@"q"];
    quit.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    [appMenu addItem:quit];
    appItem.submenu = appMenu;
    return menubar;
}

}  // namespace

Application::Application() {
    NSApplication* app = NSApplication.sharedApplication;
    GlimApplicationDelegate* delegate = [GlimApplicationDelegate new];
    app.delegate = delegate;
    app.activationPolicy = NSApplicationActivationPolicyRegular;
    app.mainMenu = makeMenu();
    application_ = (__bridge void*)app;
    delegate_ = (__bridge_retained void*)delegate;
}

Application::~Application() {
    GlimStopDisplayLink();
    if (delegate_) {
        CFRelease(delegate_);
        delegate_ = nullptr;
    }
}

void Application::run() {
    NSApplication* application = (__bridge NSApplication*)application_;
    [application finishLaunching];
    GlimEnsureDisplayLink();

    RunLoop runLoop;
    runLoop.scheduleRepeatedTask([application](RunLoop::TaskContext&) {
        while (NSEvent* event = [application nextEventMatchingMask:NSEventMaskAny
                                                         untilDate:NSDate.distantPast
                                                            inMode:NSDefaultRunLoopMode
                                                           dequeue:YES]) {
            [application sendEvent:event];
        }
    });
    runLoop.run();
    GlimStopDisplayLink();
}

}  // namespace glim::shell
