#pragma once

#ifdef __OBJC__

#import <AppKit/AppKit.h>

#else

class NSView;
class NSOpenGLContext;

#endif

namespace glim::shell::macos {
    class Surface final {
    public:
        Surface();

        Surface(const Surface &) = delete;

        Surface(Surface &&) = delete;

        Surface &operator=(const Surface &) = delete;

        Surface &operator=(Surface &&) = delete;

        ~Surface();

        void attach(NSView *contentView);

        void detach();

        void attachGlContext();

        void detachGlContext();

        void makeCurrent();

        void clearCurrent();

        void present();

        void setOpaque(bool opaque);

        void setIntervalSwap(bool enabled);

    private:
        NSView *view_;
        NSOpenGLContext *glContext_;
    };
}
