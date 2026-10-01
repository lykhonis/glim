#include <glim/shell/Surface.h>

namespace glim::shell {

Surface::Surface() {
#if defined(__APPLE__)
    info_.backend = gpu::Backend::Metal;
#else
    info_.backend = gpu::Backend::Vulkan;
#endif
}

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

void Surface::setBackend(gpu::Backend backend) {
    info_.backend = backend;
}

void Surface::setNative(int index, void* native) {
    if (index >= 0 && index < 8) {
        info_.native[index] = native;
    }
}

gpu::DeviceCreateInfo Surface::deviceCreateInfo() const {
    return info_;
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
