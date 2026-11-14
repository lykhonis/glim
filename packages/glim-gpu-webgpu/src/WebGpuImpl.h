#pragma once

#include <glim/gpu/Device.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <webgpu/webgpu.h>

namespace glim::gpu {
namespace webgpu {

inline constexpr std::uint64_t kRingBytes = 256 * 1024;

inline constexpr int kRingFrames = 3;

inline WGPUStringView strView(const char* data, std::size_t len) {
  WGPUStringView v{};
  v.data = data;
  v.length = len;
  return v;
}

struct WebBootstrap {
  WGPUInstance instance = nullptr;
  WGPUSurface surface = nullptr;
  WGPUAdapter adapter = nullptr;
  WGPUDevice device = nullptr;
  WGPUQueue queue = nullptr;
  WGPUTextureFormat format = WGPUTextureFormat_Undefined;
  bool adapterRequested = false;
  bool deviceRequested = false;
  bool ready = false;
  bool failed = false;
  std::string selector = "#glim";
  std::string error;
};

WebBootstrap& web();

void requestDevice();

void ensureInstance(const char* selector);

void ensureAdapter();

inline WGPUBuffer makeBuffer(WGPUDevice device, std::uint64_t size, WGPUBufferUsage usage) {
  WGPUBufferDescriptor bd{};
  bd.usage = usage;
  bd.size = size;
  return wgpuDeviceCreateBuffer(device, &bd);
}

inline WGPUTextureFormat srgbVariant(WGPUTextureFormat f) {
  switch (f) {
    case WGPUTextureFormat_BGRA8Unorm:
      return WGPUTextureFormat_BGRA8UnormSrgb;
    case WGPUTextureFormat_RGBA8Unorm:
      return WGPUTextureFormat_RGBA8UnormSrgb;
    default:
      return f;
  }
}

inline WGPUTextureView makeView(WGPUTexture tex) { return wgpuTextureCreateView(tex, nullptr); }

inline WGPUTextureView makeSingleMipView(WGPUTexture tex, WGPUTextureFormat format) {
  WGPUTextureViewDescriptor vd{};
  vd.format = format;
  vd.dimension = WGPUTextureViewDimension_2D;
  vd.baseMipLevel = 0;
  vd.mipLevelCount = 1;
  vd.baseArrayLayer = 0;
  vd.arrayLayerCount = 1;
  vd.aspect = WGPUTextureAspect_All;
  return wgpuTextureCreateView(tex, &vd);
}

}  // namespace webgpu

struct Device::Impl {
  std::function<void()> onPresented;
  WGPUDevice device = nullptr;
  WGPUQueue queue = nullptr;
  WGPUSurface surface = nullptr;
  // Render format: the sRGB view of the canvas storage, so linear shader
  // output is encoded on write and decoded on sample (as Metal's _sRGB).
  WGPUTextureFormat format = WGPUTextureFormat_Undefined;
  WGPUTextureFormat surfaceFormat = WGPUTextureFormat_Undefined;
  Queue queueWrapper;
  Handle next = 1;
  int drawableW = 1;
  int drawableH = 1;
  int configuredW = 0;
  int configuredH = 0;
  std::vector<WGPUShaderModule> shaders{1, nullptr};
  struct Pipe {
    WGPURenderPipeline pipe = nullptr;
  };
  std::vector<Pipe> pipelines{1};
  std::vector<WGPUBuffer> buffers{1, nullptr};
  struct Tex {
    WGPUTexture tex = nullptr;
    WGPUTextureView view = nullptr;
    int w = 0;
    int h = 0;
  };
  std::vector<Tex> textures{1};
  WGPUSampler sampler = nullptr;
  WGPUTexture dummyTex = nullptr;
  WGPUTextureView dummyView = nullptr;
  WGPUBuffer zeroBuf = nullptr;
  WGPUBuffer ring[webgpu::kRingFrames] = {nullptr, nullptr, nullptr};
  int ringIndex = 0;
  std::uint64_t ringOffset = 0;
  WGPUBindGroupLayout bindLayout = nullptr;
  WGPUPipelineLayout pipeLayout = nullptr;
  WGPUTexture swapTex = nullptr;
  WGPUTextureView swapView = nullptr;

  WGPUTexture prevTex[2] = {nullptr, nullptr};
  WGPUTextureView prevView[2] = {nullptr, nullptr};

  ~Impl() {
    releaseSwap();
    for (WGPUShaderModule s : shaders) {
      if (s != nullptr) {
        wgpuShaderModuleRelease(s);
      }
    }
    for (const Pipe& p : pipelines) {
      if (p.pipe != nullptr) {
        wgpuRenderPipelineRelease(p.pipe);
      }
    }
    for (WGPUBuffer b : buffers) {
      if (b != nullptr) {
        wgpuBufferRelease(b);
      }
    }
    for (const Tex& t : textures) {
      if (t.view != nullptr) {
        wgpuTextureViewRelease(t.view);
      }
      if (t.tex != nullptr) {
        wgpuTextureRelease(t.tex);
      }
    }
    for (WGPUBuffer b : ring) {
      if (b != nullptr) {
        wgpuBufferRelease(b);
      }
    }
    if (zeroBuf != nullptr) {
      wgpuBufferRelease(zeroBuf);
    }
    if (dummyView != nullptr) {
      wgpuTextureViewRelease(dummyView);
    }
    if (dummyTex != nullptr) {
      wgpuTextureRelease(dummyTex);
    }
    if (sampler != nullptr) {
      wgpuSamplerRelease(sampler);
    }
    if (bindLayout != nullptr) {
      wgpuBindGroupLayoutRelease(bindLayout);
    }
    if (pipeLayout != nullptr) {
      wgpuPipelineLayoutRelease(pipeLayout);
    }
    releaseSwap();
  }

  void releaseSwap() {
    if (swapView != nullptr) {
      wgpuTextureViewRelease(swapView);
      swapView = nullptr;
    }
    if (swapTex != nullptr) {
      wgpuTextureRelease(swapTex);
      swapTex = nullptr;
    }
    for (int i = 0; i < 2; ++i) {
      if (prevView[i] != nullptr) {
        wgpuTextureViewRelease(prevView[i]);
        prevView[i] = nullptr;
      }
      if (prevTex[i] != nullptr) {
        wgpuTextureRelease(prevTex[i]);
        prevTex[i] = nullptr;
      }
    }
  }

  void rotateSwap() {
    if (prevView[0] != nullptr) {
      wgpuTextureViewRelease(prevView[0]);
    }
    if (prevTex[0] != nullptr) {
      wgpuTextureRelease(prevTex[0]);
    }
    prevView[0] = prevView[1];
    prevTex[0] = prevTex[1];
    prevView[1] = swapView;
    prevTex[1] = swapTex;
    swapView = nullptr;
    swapTex = nullptr;
  }

  bool ensureCommon() {
    if (sampler != nullptr) {
      return true;
    }
    WGPUSamplerDescriptor sd{};
    sd.addressModeU = WGPUAddressMode_ClampToEdge;
    sd.addressModeV = WGPUAddressMode_ClampToEdge;
    sd.addressModeW = WGPUAddressMode_ClampToEdge;
    sd.magFilter = WGPUFilterMode_Linear;
    sd.minFilter = WGPUFilterMode_Linear;
    sd.mipmapFilter = WGPUMipmapFilterMode_Linear;
    sd.maxAnisotropy = 1;
    sampler = wgpuDeviceCreateSampler(device, &sd);
    if (sampler == nullptr) {
      return false;
    }
    WGPUTextureDescriptor td{};
    td.usage =
      static_cast<WGPUTextureUsage>(WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);
    td.dimension = WGPUTextureDimension_2D;
    td.size = {1, 1, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    dummyTex = wgpuDeviceCreateTexture(device, &td);
    if (dummyTex == nullptr) {
      return false;
    }
    dummyView = webgpu::makeView(dummyTex);
    if (dummyView == nullptr) {
      return false;
    }
    const std::uint8_t white[4] = {0xff, 0xff, 0xff, 0xff};
    WGPUTexelCopyTextureInfo dst{};
    dst.texture = dummyTex;
    dst.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferLayout layout{};
    layout.bytesPerRow = 4;
    layout.rowsPerImage = 1;
    WGPUExtent3D extent{1, 1, 1};
    wgpuQueueWriteTexture(queue, &dst, white, sizeof(white), &layout, &extent);
    zeroBuf = webgpu::makeBuffer(
      device, 64, static_cast<WGPUBufferUsage>(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst)
    );
    if (zeroBuf == nullptr) {
      return false;
    }
    for (WGPUBuffer& b : ring) {
      b = webgpu::makeBuffer(
        device,
        webgpu::kRingBytes,
        static_cast<WGPUBufferUsage>(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst)
      );
      if (b == nullptr) {
        return false;
      }
    }
    WGPUBindGroupLayoutEntry entries[kBindingCount]{};
    for (int i = 0; i < kBindingCount; ++i) {
      entries[i].binding = static_cast<std::uint32_t>(i);
      entries[i].bindingArraySize = 0;
    }
    for (int i = 0; i < 2; ++i) {
      entries[i].visibility =
        static_cast<WGPUShaderStage>(WGPUShaderStage_Vertex | WGPUShaderStage_Fragment);
      entries[i].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
      entries[i].buffer.hasDynamicOffset = WGPU_FALSE;
      entries[i].buffer.minBindingSize = 0;
    }
    for (int i = kTextureBinding; i < kSamplerBinding; ++i) {
      entries[i].visibility = WGPUShaderStage_Fragment;
      entries[i].texture.sampleType = WGPUTextureSampleType_Float;
      entries[i].texture.viewDimension = WGPUTextureViewDimension_2D;
      entries[i].texture.multisampled = WGPU_FALSE;
    }
    for (int i = kSamplerBinding; i < kBindingCount; ++i) {
      entries[i].visibility = WGPUShaderStage_Fragment;
      entries[i].sampler.type = WGPUSamplerBindingType_Filtering;
    }
    WGPUBindGroupLayoutDescriptor bld{};
    bld.entryCount = kBindingCount;
    bld.entries = entries;
    bindLayout = wgpuDeviceCreateBindGroupLayout(device, &bld);
    if (bindLayout == nullptr) {
      return false;
    }
    WGPUPipelineLayoutDescriptor pld{};
    pld.bindGroupLayoutCount = 1;
    pld.bindGroupLayouts = &bindLayout;
    pipeLayout = wgpuDeviceCreatePipelineLayout(device, &pld);
    return pipeLayout != nullptr;
  }

  void configure(int w, int h) {
    w = std::max(w, 1);
    h = std::max(h, 1);
    if (w == configuredW && h == configuredH) {
      return;
    }
    WGPUSurfaceConfiguration cfg{};
    cfg.device = device;
    cfg.format = surfaceFormat;
    cfg.viewFormatCount = format != surfaceFormat ? 1 : 0;
    cfg.viewFormats = &format;
    cfg.usage = static_cast<WGPUTextureUsage>(
      WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc
    );
    cfg.width = static_cast<std::uint32_t>(w);
    cfg.height = static_cast<std::uint32_t>(h);
    cfg.alphaMode = WGPUCompositeAlphaMode_Opaque;
    cfg.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(surface, &cfg);
    configuredW = w;
    configuredH = h;
  }

  bool alloc(std::uint64_t size, WGPUBuffer& outBuf, std::uint64_t& outOffset) {
    ringOffset = (ringOffset + 255) & ~std::uint64_t(255);
    size = (size + 15) & ~std::uint64_t(15);
    if (size > webgpu::kRingBytes) {
      return false;
    }
    if (ring[ringIndex] == nullptr || ringOffset + size > webgpu::kRingBytes) {
      ringIndex = (ringIndex + 1) % webgpu::kRingFrames;
      ringOffset = 0;
    }
    outBuf = ring[ringIndex];
    outOffset = ringOffset;
    ringOffset += size;
    return outBuf != nullptr;
  }
};

struct Pass::Impl {
  Device::Impl* dev = nullptr;
  WGPUCommandEncoder encoder = nullptr;
  WGPURenderPassEncoder pass = nullptr;
  WGPURenderPipeline pipeline = nullptr;
  bool ended = false;
  WGPUBuffer b0 = nullptr;
  std::uint64_t b0off = 0;
  std::uint64_t b0size = 0;
  WGPUBuffer b1 = nullptr;
  std::uint64_t b1off = 0;
  std::uint64_t b1size = 0;
  WGPUTextureView tex[kMaxFragmentTextures]{};
  WGPUSampler samp[kMaxFragmentSamplers]{};
};

struct CommandEncoder::Impl {
  Device* device = nullptr;
  WGPUCommandEncoder encoder = nullptr;
  bool presented = false;
};

}  // namespace glim::gpu
