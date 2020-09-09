#include <glim/shell/macos/Application.h>
#include <glim/shell/macos/RunLoop.h>

@interface GlimApplication : NSObject <NSApplicationDelegate>
@end

@implementation GlimApplication

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    [NSApp activateIgnoringOtherApps:YES];
}

@end

namespace glim::shell::macos {
    // TODO: https://github.com/glfw/glfw/blob/master/src/cocoa_init.m#L76
    NSMenu *Application::makeMenu() {
        auto menubar = [[NSMenu new] autorelease];
        auto appMenuItem = [[NSMenuItem new] autorelease];
        [menubar addItem:appMenuItem];
        auto appMenu = [[NSMenu new] autorelease];
        auto quitItem = [[[NSMenuItem alloc] initWithTitle:@"Quit"
                                                    action:@selector(terminate:)
                                             keyEquivalent:@"q"] autorelease];
        quitItem.keyEquivalentModifierMask = NSEventModifierFlagCommand;
        [appMenu addItem:quitItem];
        [appMenuItem setSubmenu:appMenu];
        return menubar;
    }

    Application::Application() {
        @autoreleasepool {
            application_ = NSApplication.sharedApplication;
            application_.delegate = delegate_ = [GlimApplication new];
            application_.activationPolicy = NSApplicationActivationPolicyRegular;
            application_.mainMenu = makeMenu();
        }
    }

    Application::~Application() {
        [delegate_ release];
    }

    void Application::run() {
        [application_ finishLaunching];
        RunLoop runLoop;
        runLoop.scheduleRepeatedTask([application = application_](RunLoop::TaskContext &context) {
            @autoreleasepool {
                while (NSEvent *event = [application nextEventMatchingMask:NSEventMaskAny
                                                                 untilDate:NSDate.distantPast
                                                                    inMode:NSDefaultRunLoopMode
                                                                   dequeue:YES]) {
                    [application sendEvent:event];
                }
            }
        });
        runLoop.run();
    }
}
