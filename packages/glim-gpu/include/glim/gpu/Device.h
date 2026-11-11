#pragma once

#include <glim/gpu/Result.h>
#include <glim/gpu/Types.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace glim::gpu {

class Device;
class Queue;
class CommandEncoder;
class Pass;

class Queue {
 public:
  Queue();
  explicit Queue(void* native);
  void* native() const { return native_; }

 private:
  void* native_ = nullptr;
};

// Pixel size of a presentable surface.
struct Extent {
  int width = 0;
  int height = 0;
};

class Drawable {
 public:
  Drawable() = default;
  Handle handle() const { return handle_; }
  int width() const { return width_; }
  int height() const { return height_; }
  void* native() const { return native_; }

  Handle handle_ = 0;
  int width_ = 0;
  int height_ = 0;
  void* native_ = nullptr;
};

class FrameTarget {
 public:
  FrameTarget() = default;
  Handle handle() const { return handle_; }
  int width() const { return width_; }
  int height() const { return height_; }
  void* native() const { return native_; }

  Handle handle_ = 0;
  int width_ = 0;
  int height_ = 0;
  void* native_ = nullptr;
};

class Buffer {
 public:
  Buffer() = default;
  Handle handle() const { return handle_; }
  void* native() const { return native_; }
  std::uint64_t size() const { return size_; }

  Handle handle_ = 0;
  void* native_ = nullptr;
  std::uint64_t size_ = 0;
};

class Texture {
 public:
  Texture() = default;
  Handle handle() const { return handle_; }
  void* native() const { return native_; }
  int width() const { return width_; }
  int height() const { return height_; }

  Handle handle_ = 0;
  void* native_ = nullptr;
  int width_ = 0;
  int height_ = 0;
};

class Shader {
 public:
  Shader() = default;
  Handle handle() const { return handle_; }
  void* native() const { return native_; }

  Handle handle_ = 0;
  void* native_ = nullptr;
};

class Pipeline {
 public:
  Pipeline() = default;
  Handle handle() const { return handle_; }
  void* native() const { return native_; }

  Handle handle_ = 0;
  void* native_ = nullptr;
};

class Bindings {
 public:
  Bindings() = default;
  Handle handle() const { return handle_; }

  Handle handle_ = 0;
};

class Pass {
 public:
  Pass();
  ~Pass();
  Pass(Pass&&) noexcept;
  Pass& operator=(Pass&&) noexcept;
  Pass(const Pass&) = delete;
  Pass& operator=(const Pass&) = delete;

  void setPipeline(const Pipeline&);
  void setBindings(std::uint32_t index, const Bindings&);
  void setVertexBuffer(std::uint32_t index, const Buffer&, std::uint64_t offset);
  void setBytes(std::uint32_t index, const void* data, std::uint64_t size);
  void setFragmentBytes(std::uint32_t index, const void* data, std::uint64_t size);
  void setFragmentTexture(std::uint32_t index, void* nativeTexture);
  void setFragmentSampler(std::uint32_t index, void* nativeSampler);
  void setViewport(float x, float y, float w, float h, float z0, float z1);
  void setScissor(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h);
  void draw(
      std::uint32_t vertexCount,
      std::uint32_t instanceCount,
      std::uint32_t firstVertex,
      std::uint32_t firstInstance
  );
  void drawIndexed(
      std::uint32_t indexCount,
      std::uint32_t instanceCount,
      std::uint32_t firstIndex,
      std::int32_t baseVertex,
      std::uint32_t firstInstance
  );
  void end();

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

class CommandEncoder {
 public:
  CommandEncoder();
  ~CommandEncoder();
  CommandEncoder(CommandEncoder&&) noexcept;
  CommandEncoder& operator=(CommandEncoder&&) noexcept;
  CommandEncoder(const CommandEncoder&) = delete;
  CommandEncoder& operator=(const CommandEncoder&) = delete;

  Pass beginPass(const PassDesc&);
  bool copyColorTo(const FrameTarget& dst);
  void generateMips(const FrameTarget& dst);
  void present(const Drawable&);
  void submit(Queue&);

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

class Device {
 public:
  static Result<Device> create(const DeviceCreateInfo&);

  Device();
  ~Device();
  Device(Device&&) noexcept;
  Device& operator=(Device&&) noexcept;
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  Queue& queue();
  Result<Drawable> nextDrawable();
  // Size the next drawable will have, without acquiring it; {0, 0} when
  // unknown. Lets a caller skip a frame before taking a swapchain image.
  Extent drawableSize() const;
  void setDrawableSize(int width, int height);
  int presentRotationDegrees() const;
  // GPU execution time of the latest completed submit, ms; 0 when the
  // backend does not measure it. Completion is asynchronous, so during
  // a frame this is usually an earlier frame's cost.
  float lastGpuMs() const;
  void notifyNextPresented(std::function<void()> fn);
  Result<FrameTarget> createFrameTarget(const FrameTargetDesc&);
  bool headless() const;
  bool readPixels(const FrameTarget&, void* rgba8, std::uint64_t bytes);
  Result<Buffer> createBuffer(const BufferDesc&);
  void writeBuffer(Buffer&, const void* data, std::uint64_t size);
  Result<Texture> createTexture(const TextureDesc&);
  void writeTexture(Texture&, const void* rgba8, std::uint64_t bytes);
  Result<Texture> wrapNativeTexture(void* native, int width, int height);
  Result<Texture> importSurface(void* surface, int width, int height, SampleFormat);
  Result<Shader> createShader(ShaderStage, const char* sourceUtf8, std::uint64_t size);
  std::vector<Result<Shader>> createShaders(std::span<const ShaderSource> sources);
  void prepareShaders(std::span<const ShaderSource> sources);
  Result<Pipeline> createPipeline(const PipelineDesc&);
  Result<Bindings> createBindings(const BindingsDesc&);
  CommandEncoder encoder();

  void* nativeDevice() const;
  void* nativeLayer() const;
  void* nativeSampler() const;
  void* colorNative() const;

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

inline std::vector<Result<Shader>> createShadersInOrder(
    Device& device, std::span<const ShaderSource> sources
) {
  std::vector<Result<Shader>> out;
  out.reserve(sources.size());
  for (const ShaderSource& s : sources) out.push_back(device.createShader(s.stage, s.code, s.size));
  return out;
}

}  // namespace glim::gpu
