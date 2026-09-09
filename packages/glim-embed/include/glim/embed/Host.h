#pragma once

#include <glim/gpu/Types.h>
#include <glim/math.h>
#include <glim/paint/FramePacket.h>
#include <glim/shell/Event.h>

namespace glim::embed {

// Contract for a native embedder (TV compositor, Android Automotive / QNX HMI,
// browser canvas, desktop window). Glim never owns the OS process.
//
// Two process shapes, same paint:
//
// 1. In-process C++: Host builds Device from deviceCreateInfo(), runs
//    Context → encode() → submit on one thread.
// 2. WASM guest: core + paint compile to wasm32 and emit a FramePacket once
//    per frame. Host stays native C++ (this struct + gpu backend + present).
//    Do not import GPU calls per quad — that FFI will not hold 60/120 Hz on a
//    vehicle SoC.
//
// The guest must not include this header (it pulls shell Event). Hosts do.

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

    // Optional: embedder-driven loop (cars/TVs often have their own).
    virtual void inject(const shell::Event&) {}
};

}  // namespace glim::embed
