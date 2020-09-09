#pragma once

#include <string>
#include <functional>

#ifdef __OBJC__

#import <AppKit/AppKit.h>

#else
class NSWindow;
class NSView;
#endif

#include <glim/shell/macos/Event.h>

namespace glim::shell::macos {
    class Window final {
    public:
        typedef std::function<void(Event)> EventCallback;

        Window();

        Window(const Window &) = delete;

        Window(Window &&) = delete;

        Window &operator=(const Window &) = delete;

        Window &operator=(Window &&) = delete;

        ~Window();

        void setSize(int width, int height);

        void setTitle(const std::string &);

        void center();

        void show();

        void setEventCallback(EventCallback);

        NSView *view() {
            return view_;
        }

    private:
        NSWindow *window_;
        NSView *view_;
    };
}
