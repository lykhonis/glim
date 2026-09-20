#include <glim/shell/Window.h>

namespace glim::shell {

Window::Window() = default;
Window::~Window() = default;

void Window::setSize(int width, int height) {
    if (width > 0) {
        width_ = width;
    }
    if (height > 0) {
        height_ = height;
    }
}

void Window::setPixelRatio(float ratio) {
    if (ratio > 0.f) {
        pixelRatio_ = ratio;
    }
}

void Window::setTitle(const std::string& title) {
    title_ = title;
}

void Window::center() {}

void Window::show() {
    shown_ = true;
}

void Window::hide() {
    shown_ = false;
}

void Window::setEventCallback(EventCallback callback) {
    callback_ = std::move(callback);
}

void Window::attachToScene(void*) {}

Vec2 Window::size() const {
    return Vec2{static_cast<float>(width_), static_cast<float>(height_)};
}

Vec2 Window::drawableSize() const {
    return Vec2{static_cast<float>(width_) * pixelRatio_,
                static_cast<float>(height_) * pixelRatio_};
}

float Window::pixelRatio() const {
    return pixelRatio_;
}

Rect Window::safeArea() const {
    return Rect::fromSize(size());
}

void* Window::nativeView() const {
    return nullptr;
}

void Window::attachSlot(std::uint32_t id, SlotNative native) {
    slots_.attach(id, native.view);
}

void Window::positionSlot(std::uint32_t id, Rect windowLogical) {
    (void)id;
    (void)windowLogical;
}

void Window::detachSlot(std::uint32_t id) {
    slots_.detach(id);
}

#if GLIM_SOFTWARE
std::uint8_t* Window::mapSoftware(int, int) {
    return nullptr;
}
void Window::presentSoftware() {}
#endif

void Window::dispatch(const Event& event) {
    if (callback_) {
        callback_(event);
    }
}

void Window::setCanvasSelector(const std::string& selector) {
    if (!selector.empty()) {
        canvasSelector_ = selector;
    }
}

}  // namespace glim::shell
