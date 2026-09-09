#include <glim/shell/Application.h>
#include <glim/shell/Event.h>
#include <glim/shell/RunLoop.h>
#include <glim/shell/Window.h>

#include "WindowRegistry.h"

#import <AppKit/AppKit.h>

@interface GlimApplicationDelegate : NSObject <NSApplicationDelegate>
@end

@implementation GlimApplicationDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    [NSApp activateIgnoringOtherApps:YES];
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
    if (delegate_) {
        CFRelease(delegate_);
        delegate_ = nullptr;
    }
}

void Application::run() {
    NSApplication* application = (__bridge NSApplication*)application_;
    [application finishLaunching];

    RunLoop runLoop;
    runLoop.scheduleRepeatedTask([application](RunLoop::TaskContext&) {
        while (NSEvent* event = [application nextEventMatchingMask:NSEventMaskAny
                                                         untilDate:NSDate.distantPast
                                                            inMode:NSDefaultRunLoopMode
                                                           dequeue:YES]) {
            [application sendEvent:event];
        }
    });
    runLoop.scheduleFrameCallback([] {
        const Event frame(EventType::Frame);
        for (Window* window : detail::shownWindows()) {
            window->dispatch(frame);
        }
    });
    runLoop.run();
}

}  // namespace glim::shell
