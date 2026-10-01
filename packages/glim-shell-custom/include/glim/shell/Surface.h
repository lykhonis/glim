#pragma once

#include <glim/gpu/Device.h>
#include <glim/math.h>
#include <glim/shell/Window.h>

namespace glim::shell {

// Custom surface: binds a Window to an embedder-supplied GPU device.
// The OEM provides natives via setNative()/setBackend() before
// Device::create(deviceCreateInfo()).
class Surface final {
public:
    Surface();
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    ~Surface();

    void attach(Window&);
    void detach();
    void setVSync(bool enabled);
    void setOpaque(bool opaque);
    void setBackend(gpu::Backend backend);
    void setNative(int index, void* native);

    gpu::DeviceCreateInfo deviceCreateInfo() const;
    Vec2 drawableSize() const;
    float pixelRatio() const;
    void* nativeLayer() const;

private:
    Window* window_ = nullptr;
    bool vsync_ = true;
    bool opaque_ = true;
    gpu::DeviceCreateInfo info_{};
};

}  // namespace glim::shell
