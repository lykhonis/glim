#pragma once

#include <glim/gpu/Device.h>
#include <glim/math.h>
#include <glim/shell/Window.h>

namespace glim::shell {

// Binds a Window canvas to WebGPU; see docs/webgpu.md for the native layout.
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

  gpu::DeviceCreateInfo deviceCreateInfo() const;
  Vec2 drawableSize() const;
  float pixelRatio() const;
  void* nativeLayer() const;

 private:
  Window* window_ = nullptr;
  bool vsync_ = true;
  bool opaque_ = true;
};

}  // namespace glim::shell
