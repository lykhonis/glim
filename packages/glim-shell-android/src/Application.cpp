#include <glim/shell/Application.h>

#include "App.h"
#include "WindowRegistry.h"

#include <glim/shell/Event.h>
#include <glim/shell/RunLoop.h>
#include <glim/shell/Window.h>

#include <android/choreographer.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android_native_app_glue.h>

#include <algorithm>
#include <cmath>

namespace glim::gpu {
void setAndroidNativeWindow(void* native);
}

extern "C" int glim_app_main();

namespace glim::shell {
namespace detail_app {

bool gChoreoActive = false;

Key mapKey(int32_t code) {
    switch (code) {
        case AKEYCODE_ENTER:
            return Key::Enter;
        case AKEYCODE_ESCAPE:
            return Key::Escape;
        case AKEYCODE_BACK:
        case AKEYCODE_DEL:
            return Key::Back;
        case AKEYCODE_DPAD_UP:
            return Key::DpadUp;
        case AKEYCODE_DPAD_DOWN:
            return Key::DpadDown;
        case AKEYCODE_DPAD_LEFT:
            return Key::DpadLeft;
        case AKEYCODE_DPAD_RIGHT:
            return Key::DpadRight;
        case AKEYCODE_DPAD_CENTER:
            return Key::DpadCenter;
        default:
            return Key::Unknown;
    }
}

void dispatchAll(const Event& event) {
    for (Window* window : detail::shownWindows()) {
        window->dispatch(event);
    }
}

void dispatchResize() {
    const float r = detail::pixelRatio();
    Event e(EventType::WindowResized);
    e.setSize(std::max(1, static_cast<int>(std::lround(static_cast<float>(detail::drawableWidth()) / r))),
              std::max(1, static_cast<int>(std::lround(static_cast<float>(detail::drawableHeight()) / r))));
    dispatchAll(e);
}

void onChoreographerFrame(long, void*);

void postChoreographer() {
    if (!gChoreoActive) {
        return;
    }
    AChoreographer* choreographer = AChoreographer_getInstance();
    if (!choreographer) {
        return;
    }
    AChoreographer_postFrameCallback(choreographer, onChoreographerFrame, nullptr);
}

void onChoreographerFrame(long, void*) {
    if (!gChoreoActive) {
        return;
    }
    dispatchAll(Event(EventType::Frame));
    postChoreographer();
}

void setChoreographer(bool active) {
    if (gChoreoActive == active) {
        if (active) {
            postChoreographer();
        }
        return;
    }
    gChoreoActive = active;
    if (active) {
        postChoreographer();
    }
}

void onAppCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            gpu::setAndroidNativeWindow(app->window);
            dispatchResize();
            setChoreographer(true);
            break;
        case APP_CMD_TERM_WINDOW:
            setChoreographer(false);
            gpu::setAndroidNativeWindow(nullptr);
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONTENT_RECT_CHANGED:
        case APP_CMD_CONFIG_CHANGED:
            dispatchResize();
            break;
        case APP_CMD_GAINED_FOCUS:
            setChoreographer(true);
            break;
        case APP_CMD_LOST_FOCUS:
        case APP_CMD_PAUSE:
            setChoreographer(false);
            break;
        case APP_CMD_RESUME:
            if (app->window) {
                setChoreographer(true);
            }
            break;
        case APP_CMD_DESTROY:
            dispatchAll(Event(EventType::WindowClosed));
            RunLoop().stop();
            break;
        default:
            break;
    }
}

void emitPointer(EventType type, AInputEvent* event, int32_t index) {
    const float r = detail::pixelRatio();
    Event e(type);
    e.setPoint(AMotionEvent_getX(event, static_cast<size_t>(index)) / r,
               AMotionEvent_getY(event, static_cast<size_t>(index)) / r);
    e.setButton(PointerButton::Left);
    e.setPointerId(static_cast<int>(AMotionEvent_getPointerId(event, static_cast<size_t>(index))));
    dispatchAll(e);
}

int32_t onInputEvent(android_app*, AInputEvent* event) {
    const int32_t type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_KEY) {
        const int32_t action = AKeyEvent_getAction(event);
        const Key key = mapKey(AKeyEvent_getKeyCode(event));
        if (key == Key::Unknown) {
            return 0;
        }
        Event e(action == AKEY_EVENT_ACTION_UP ? EventType::KeyUp : EventType::KeyDown);
        e.setKey(key);
        dispatchAll(e);
        return 0;
    }
    if (type != AINPUT_EVENT_TYPE_MOTION) {
        return 0;
    }
    const int32_t action = AMotionEvent_getAction(event);
    const int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
    const int32_t index =
        (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    switch (masked) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            emitPointer(EventType::PointerDown, event, index);
            return 1;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
        case AMOTION_EVENT_ACTION_CANCEL:
            emitPointer(EventType::PointerUp, event, index);
            return 1;
        case AMOTION_EVENT_ACTION_MOVE: {
            const size_t n = AMotionEvent_getPointerCount(event);
            for (size_t i = 0; i < n; ++i) {
                emitPointer(EventType::PointerMove, event, static_cast<int32_t>(i));
            }
            return 1;
        }
        default:
            return 0;
    }
}

void pumpUntilWindow(android_app* app) {
    while (!app->destroyRequested && app->window == nullptr) {
        android_poll_source* source = nullptr;
        const int ident = ALooper_pollOnce(-1, nullptr, nullptr, reinterpret_cast<void**>(&source));
        if (ident == ALOOPER_POLL_ERROR) {
            break;
        }
        if (source) {
            source->process(app, source);
        }
    }
}

}  // namespace detail_app

using detail_app::setChoreographer;

Application::Application() = default;

Application::~Application() = default;

void Application::run() {
    android_app* app = detail::androidApp();
    if (!app) {
        return;
    }
    setChoreographer(app->window != nullptr);
    RunLoop().run();
    setChoreographer(false);
}

}  // namespace glim::shell

extern "C" __attribute__((visibility("default"))) void android_main(android_app* app) {
    glim::shell::detail::setAndroidApp(app);
    app->onAppCmd = glim::shell::detail_app::onAppCmd;
    app->onInputEvent = glim::shell::detail_app::onInputEvent;
    glim::shell::detail_app::pumpUntilWindow(app);
    if (app->destroyRequested || !app->window) {
        return;
    }
    glim_app_main();
    glim::shell::detail_app::setChoreographer(false);
    glim::gpu::setAndroidNativeWindow(nullptr);
    glim::shell::detail::setAndroidApp(nullptr);
}
