#pragma once

#include <cstdint>
#include <memory>

#include <glim/gpu/Result.h>
#include <glim/gpu/Types.h>

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

    Handle handle_ = 0;
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
    void draw(std::uint32_t vertexCount, std::uint32_t instanceCount, std::uint32_t firstVertex,
              std::uint32_t firstInstance);
    void drawIndexed(std::uint32_t indexCount, std::uint32_t instanceCount, std::uint32_t firstIndex,
                     std::int32_t baseVertex, std::uint32_t firstInstance);
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
    Result<FrameTarget> createFrameTarget(const FrameTargetDesc&);
    Result<Buffer> createBuffer(const BufferDesc&);
    void writeBuffer(Buffer&, const void* data, std::uint64_t size);
    Result<Texture> createTexture(const TextureDesc&);
    Result<Shader> createShader(ShaderStage, const char* sourceUtf8, std::uint64_t size);
    Result<Pipeline> createPipeline(const PipelineDesc&);
    Result<Bindings> createBindings(const BindingsDesc&);
    CommandEncoder encoder();

    void* nativeDevice() const;
    void* nativeLayer() const;
    void* nativeSampler() const;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace glim::gpu
