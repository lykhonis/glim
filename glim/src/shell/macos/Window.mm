#include <glim/shell/macos/Window.h>

using namespace glim::shell::macos;

@interface GlimView : NSView <NSWindowDelegate>

- (void)setEventCallback:(Window::EventCallback)callback;

@end

@implementation GlimView {
    Window::EventCallback eventCallback_;
}

- (void)setEventCallback:(Window::EventCallback)callback {
    eventCallback_ = std::move(callback);
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

- (void)windowWillClose:(NSNotification *)notification {
    if (eventCallback_) {
        eventCallback_(Event(Event::Type::WindowClosed));
    }
}

@end

namespace glim::shell::macos {

    Window::Window() {
        @autoreleasepool {
            window_ = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 380)
                                                  styleMask:NSWindowStyleMaskTitled
                                                            | NSWindowStyleMaskClosable
                                                            | NSWindowStyleMaskMiniaturizable
                                                            | NSWindowStyleMaskResizable
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
            view_ = [[GlimView alloc] initWithFrame:window_.contentView.frame];
            view_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
            window_.delegate = static_cast<id <NSWindowDelegate>>(view_);
            window_.releasedWhenClosed = NO;
            [window_.contentView addSubview:view_];
            [window_ makeFirstResponder:view_];
        }
    }

    Window::~Window() {
        @autoreleasepool {
            [view_ removeFromSuperview];
            [view_ release];
            [window_ release];
        }
    }

    void Window::show() {
        @autoreleasepool {
            [window_ makeKeyAndOrderFront:nil];
        }
    }

    void Window::setSize(int width, int height) {
        @autoreleasepool {
            [window_ setContentSize:NSMakeSize(width, height)];
        }
    }

    void Window::setTitle(const std::string &title) {
        @autoreleasepool {
            NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
            [window_ setTitle:nsTitle];
        }
    }

    void Window::center() {
        @autoreleasepool {
            [window_ center];
        }
    }

    void Window::setEventCallback(EventCallback callback) {
        @autoreleasepool {
            [static_cast<GlimView *>(view_) setEventCallback:std::move(callback)];
        }
    }
}
