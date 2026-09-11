#include <glim/shell/Window.h>

#include "App.h"
#include "WindowRegistry.h"

#include <algorithm>

namespace glim::shell {

struct WindowImpl {
    Window::EventCallback callback;
};

Window::Window() {
    auto* impl = new WindowImpl();
    window_ = impl;
    view_ = detail::nativeWindow();
}

Window::~Window() {
    detail::unregisterShownWindow(this);
    delete static_cast<WindowImpl*>(window_);
    window_ = nullptr;
    view_ = nullptr;
}

void Window::show() {
    view_ = detail::nativeWindow();
    if (!shown_) {
        shown_ = true;
        detail::registerShownWindow(this);
    }
}

void Window::hide() {
    shown_ = false;
    detail::unregisterShownWindow(this);
}

void Window::setSize(int, int) {
    // ANativeWindow is host-owned on phones, TV, and AAOS.
}

void Window::setTitle(const std::string&) {}

void Window::center() {}

void Window::setEventCallback(EventCallback callback) {
    auto* impl = static_cast<WindowImpl*>(window_);
    if (impl) {
        impl->callback = std::move(callback);
    }
}

Vec2 Window::size() const {
    const float r = pixelRatio();
    const Vec2 drawable = drawableSize();
    return {drawable.x / r, drawable.y / r};
}

float Window::pixelRatio() const {
    return detail::pixelRatio();
}

Vec2 Window::drawableSize() const {
    return {static_cast<float>(detail::drawableWidth()), static_cast<float>(detail::drawableHeight())};
}

Rect Window::safeArea() const {
    android_app* app = detail::androidApp();
    const Vec2 logical = size();
    if (!app) {
        return Rect::fromSize(logical);
    }
    const ARect& c = app->contentRect;
    if (c.right <= c.left || c.bottom <= c.top) {
        return Rect::fromSize(logical);
    }
    const float r = pixelRatio();
    const float x = static_cast<float>(c.left) / r;
    const float y = static_cast<float>(c.top) / r;
    const float w = static_cast<float>(c.right - c.left) / r;
    const float h = static_cast<float>(c.bottom - c.top) / r;
    return {{x, y}, {std::max(0.0f, w), std::max(0.0f, h)}};
}

void* Window::nativeView() const {
    return detail::nativeWindow();
}

void Window::dispatch(const Event& event) {
    auto* impl = static_cast<WindowImpl*>(window_);
    if (impl && impl->callback) {
        impl->callback(event);
    }
}

}  // namespace glim::shell
