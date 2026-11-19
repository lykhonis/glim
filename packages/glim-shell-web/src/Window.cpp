#include <glim/shell/Window.h>

#include <algorithm>
#include <vector>

#include "Shown.h"

namespace glim::shell {

namespace detail {

std::vector<Window*>& shownWindows() {
  static std::vector<Window*> shown;
  return shown;
}

}  // namespace detail

Window::Window() = default;
Window::~Window() { hide(); }

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

void Window::setTitle(const std::string& title) { title_ = title; }

void Window::center() {}

void Window::setCustomTitlebar(bool) {}

void Window::dragWindow() {}

void Window::show() {
  if (!shown_) {
    detail::shownWindows().push_back(this);
    detail::listen(this, canvasSelector_);
  }
  shown_ = true;
}

void Window::hide() {
  auto& shown = detail::shownWindows();
  shown.erase(std::remove(shown.begin(), shown.end(), this), shown.end());
  shown_ = false;
}

void Window::setEventCallback(EventCallback callback) { callback_ = std::move(callback); }

void Window::attachToScene(void*) {}

Vec2 Window::size() const { return Vec2{static_cast<float>(width_), static_cast<float>(height_)}; }

Vec2 Window::drawableSize() const {
  return Vec2{static_cast<float>(width_) * pixelRatio_, static_cast<float>(height_) * pixelRatio_};
}

float Window::pixelRatio() const { return pixelRatio_; }

Rect Window::safeArea() const { return Rect::fromSize(size()); }

void* Window::nativeView() const { return nullptr; }

void Window::attachSlot(std::uint32_t id, SlotNative native) { slots_.attach(id, native.view); }

void Window::positionSlot(std::uint32_t id, Rect windowLogical) {
  if (id == 0 || windowLogical.size.x <= 0.f || windowLogical.size.y <= 0.f) {
    return;
  }
  slotRects_[id] = windowLogical;
}

void Window::positionSlot(std::uint32_t id, const SlotFrame& frame) {
  positionSlot(id, frame.bounds);
}

void Window::detachSlot(std::uint32_t id) {
  slots_.detach(id);
  slotRects_.erase(id);
}

#if GLIM_SOFTWARE
std::uint8_t* Window::mapSoftware(int, int) { return nullptr; }
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

void Window::present(const PresentRequest&, PresentDone done) {
  done({PresentStatus::Unsupported, {}});
}

void Window::dismissPresented() {}

void Window::setA11y(std::vector<A11yElement>) {}

// Owned text input lands per shell after macOS (N8).
void Window::setA11yPerform(A11yPerform) {}

bool Window::a11yActive() const { return false; }

}  // namespace glim::shell
