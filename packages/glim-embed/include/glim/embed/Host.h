#pragma once

#include <glim/gpu/Types.h>
#include <glim/math.h>
#include <glim/shell/Event.h>

namespace glim::embed {

struct HostFrame {
    Vec2 logicalSize;
    Vec2 drawableSize;
    float pixelRatio = 1.0f;
};

struct Host {
    virtual ~Host() = default;
    virtual gpu::DeviceCreateInfo deviceCreateInfo() = 0;
    virtual HostFrame frame() const = 0;
    virtual void setVSync(bool enabled) = 0;
    virtual void present() = 0;
    virtual void inject(const shell::Event&) {}
};

}  // namespace glim::embed
