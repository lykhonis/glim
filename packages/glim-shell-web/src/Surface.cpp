#include <glim/shell/Surface.h>

namespace glim::shell {

Surface::Surface() = default;
Surface::~Surface() = default;

void Surface::attach(Window& window) {
    window_ = &window;
}

void Surface::detach() {
    window_ = nullptr;
}

void Surface::setVSync(bool enabled) {
    vsync_ = enabled;
}

void Surface::setOpaque(bool opaque) {
    opaque_ = opaque;
}

gpu::DeviceCreateInfo Surface::deviceCreateInfo() const {
    gpu::DeviceCreateInfo info{};
    info.backend = gpu::Backend::WebGpu;
    info.native[0] = nullptr;
    info.native[1] = nullptr;
    info.native[2] = vsync_ ? reinterpret_cast<void*>(1) : nullptr;
    return info;
}

Vec2 Surface::drawableSize() const {
    if (window_ == nullptr) {
        return Vec2{0.0f, 0.0f};
    }
    return window_->drawableSize();
}

float Surface::pixelRatio() const {
    if (window_ == nullptr) {
        return 1.0f;
    }
    return window_->pixelRatio();
}

void* Surface::nativeLayer() const {
    return nullptr;
}

}  // namespace glim::shell
