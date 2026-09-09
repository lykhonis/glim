#pragma once

#include <cstdint>

namespace glim::gpu {

enum class Backend { Metal, Vulkan, WebGpu };

using Handle = std::uint32_t;

enum class ShaderStage { Vertex, Fragment };

enum class Blend { SrcOver, Plus };

enum class LoadOp { DontCare, Clear, Load };
enum class StoreOp { Store, DontCare };

enum class IndexType { U16, U32 };

struct ColorClear {
    float rgba[4]{0, 0, 0, 0};
};

// native[] is filled by the embedder (CAMetalLayer, VkSwapchain, WebGPU surface,
// or a vehicle compositor's display). Paint/encode never dereference these.
struct DeviceCreateInfo {
    Backend backend = Backend::Metal;
    void* native[8]{};
};

struct BufferDesc {
    std::uint64_t size = 0;
    bool vertex = false;
    bool uniform = false;
    bool index = false;
};

struct TextureDesc {
    int width = 1;
    int height = 1;
};

struct FrameTargetDesc {
    int width = 1;
    int height = 1;
};

struct PipelineDesc {
    Handle vertexShader = 0;
    Handle fragmentShader = 0;
    Blend blend = Blend::SrcOver;
};

struct BindingsDesc {
    Handle buffer = 0;
    std::uint32_t index = 0;
};

struct PassDesc {
    Handle color = 0;
    void* nativeColor = nullptr;  // MTLTexture / VkImage view; null = current drawable
    LoadOp load = LoadOp::Clear;
    StoreOp store = StoreOp::Store;
    ColorClear clear{};
    int viewportW = 0;
    int viewportH = 0;
};

}  // namespace glim::gpu
