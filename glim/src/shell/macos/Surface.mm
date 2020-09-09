#include <glim/shell/macos/Surface.h>
#include <glim/utils/Assert.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace glim::shell::macos {

    Surface::Surface() : view_(nil), glContext_(nil) {
        view_ = [[NSView alloc] initWithFrame:{{0, 0},
                                               {0, 0}}];
    }

    Surface::~Surface() {
        detach();
        @autoreleasepool {
            [glContext_ release];
            [view_ release];
        }
    }

    void Surface::attach(NSView *contentView) {
        detach();
        @autoreleasepool {
            [contentView addSubview:view_];
            view_.frame = contentView.bounds;
            if (glContext_) {
                [glContext_ update];
            }
        }
    }

    void Surface::detach() {
        @autoreleasepool {
            [view_ removeFromSuperview];
        }
    }

    void Surface::attachGlContext() {
        @autoreleasepool {
            if (!glContext_) {
                static const NSOpenGLPixelFormatAttribute attributes[] = {
                        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
                        NSOpenGLPFAAccelerated,
                        NSOpenGLPFAClosestPolicy,
                        NSOpenGLPFADoubleBuffer,
                        NSOpenGLPFAColorSize, 24,
                        NSOpenGLPFAAlphaSize, 8,
                        NSOpenGLPFADepthSize, 0,
                        NSOpenGLPFAStencilSize, 0,
                        0,
                };
                auto format = [[[NSOpenGLPixelFormat alloc] initWithAttributes:attributes] autorelease];
                glContext_ = [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
//                [view_ setWantsBestResolutionOpenGLSurface:YES];
            }
            glContext_.view = view_;
        }
    }

    void Surface::detachGlContext() {
        @autoreleasepool {
            if (glContext_) {
                glContext_.view = nil;
                [glContext_ release];
                glContext_ = nil;
            }
        }
    }

    void Surface::setOpaque(bool opaque) {
        @autoreleasepool {
            GLIM_ASSERT(glContext_, "GL context is not attached");
            GLint opacity = opaque ? 1 : 0;
            [glContext_ setValues:&opacity forParameter:NSOpenGLContextParameterSurfaceOpacity];
        }
    }

    void Surface::setIntervalSwap(bool enabled) {
        @autoreleasepool {
            GLIM_ASSERT(glContext_, "GL context is not attached");
            GLint swapInterval = enabled ? 1 : 0;
            [glContext_ setValues:&swapInterval forParameter:NSOpenGLContextParameterSwapInterval];
        }
    }

    void Surface::makeCurrent() {
        @autoreleasepool {
            GLIM_ASSERT(glContext_, "GL context is not attached");
            [glContext_ makeCurrentContext];
        }
    }

    void Surface::clearCurrent() {
        @autoreleasepool {
            GLIM_ASSERT(glContext_, "GL context is not attached");
            [NSOpenGLContext clearCurrentContext];
        }
    }

    void Surface::present() {
        @autoreleasepool {
            GLIM_ASSERT(glContext_, "GL context is not attached");
            [glContext_ flushBuffer];
        }
    }

//    static GLFWglproc getProcAddressNSGL(const char* procname)
//    {
//        CFStringRef symbolName = CFStringCreateWithCString(kCFAllocatorDefault,
//                                                           procname,
//                                                           kCFStringEncodingASCII);
//
//        GLFWglproc symbol = CFBundleGetFunctionPointerForName(_glfw.nsgl.framework,
//                                                              symbolName);
//
//        CFRelease(symbolName);
//
//        return symbol;
//    }
}

#pragma clang diagnostic pop