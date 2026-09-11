#include <glim/shell/Surface.h>

#include <cstdint>

namespace glim::shell {

Surface::Surface() = default;

Surface::~Surface() {
    detach();
}

void Surface::attach(Window& window) {
    detach();
    window_ = &window;
}

void Surface::detach() {
    window_ = nullptr;
}

void Surface::setVSync(bool enabled) {
    vsync_ = enabled;
}

void Surface::setOpaque(bool) {}

gpu::DeviceCreateInfo Surface::deviceCreateInfo() const {
    gpu::DeviceCreateInfo info;
    info.backend = gpu::Backend::Vulkan;
    info.native[0] = window_ ? window_->nativeView() : nullptr;
    info.native[2] = vsync_ ? reinterpret_cast<void*>(static_cast<uintptr_t>(1)) : nullptr;
    return info;
}

Vec2 Surface::drawableSize() const {
    return window_ ? window_->drawableSize() : Vec2{};
}

float Surface::pixelRatio() const {
    return window_ ? window_->pixelRatio() : 1.0f;
}

void* Surface::nativeLayer() const {
    return window_ ? window_->nativeView() : nullptr;
}

}  // namespace glim::shell
