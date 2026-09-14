#pragma once

#include <glim/gpu/Types.h>
#include <glim/math.h>
#include <glim/shell/Event.h>
#include <glim/shell/Slot.h>

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
    virtual void attachSlot(std::uint32_t, shell::SlotNative) {}
    virtual void positionSlot(std::uint32_t, Rect) {}
    virtual void detachSlot(std::uint32_t) {}
};

}  // namespace glim::embed
