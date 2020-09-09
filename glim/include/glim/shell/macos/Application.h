#pragma once

#ifdef __OBJC__

#import <AppKit/AppKit.h>

#else
template<typename T>
using id = void *;

class NSApplication;

class NSApplicationDelegate;

class NSMenu;

#endif

namespace glim::shell::macos {
    class Application final {
    public:
        Application();

        Application(const Application &) = delete;

        Application(Application &&) = delete;

        Application &operator=(const Application &) = delete;

        Application &operator=(Application &&) = delete;

        ~Application();

        void run();

    private:
        NSMenu *makeMenu();

        id<NSApplicationDelegate> delegate_;
        NSApplication *application_;
    };
}
