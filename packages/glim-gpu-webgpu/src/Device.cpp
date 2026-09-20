#include <glim/gpu/Device.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <webgpu/webgpu.h>

namespace glim::gpu {
namespace {

constexpr std::uint64_t kRingBytes = 256 * 1024;
constexpr int kRingFrames = 3;

WGPUStringView strView(const char* data, std::size_t len) {
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

WebBootstrap& web() {
    static WebBootstrap s;
    return s;
}

void onDeviceLost(const WGPUDevice* device, WGPUDeviceLostReason reason, WGPUStringView message,
                  void* userdata1, void* userdata2) {
    (void)device;
    (void)reason;
    (void)message;
    (void)userdata1;
    (void)userdata2;
    WebBootstrap& w = web();
    printf("[glim-web] device lost reason=%d\n", (int)reason);
    w.ready = false;
    w.deviceRequested = false;
    w.device = nullptr;
    w.queue = nullptr;
}

void onDevice(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message,
              void* userdata1, void* userdata2) {
    (void)userdata1;
    (void)userdata2;
    WebBootstrap& w = web();
    if (status != WGPURequestDeviceStatus_Success || device == nullptr) {
        w.failed = true;
        w.error = "webgpu requestDevice failed";
        if (message.data != nullptr && message.length != WGPU_STRLEN && message.length > 0) {
            w.error += ": ";
            w.error += std::string(message.data, message.length);
        }
        return;
    }
    w.device = device;
    w.queue = wgpuDeviceGetQueue(device);
    WGPUSurfaceCapabilities caps{};
    if (wgpuSurfaceGetCapabilities(w.surface, w.adapter, &caps) != WGPUStatus_Success ||
        caps.formatCount == 0) {
        w.failed = true;
        w.error = "webgpu surface has no formats";
        return;
    }
    w.format = caps.formats[0];
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    w.ready = true;
}

void onUncaptured(const WGPUDevice* device, WGPUErrorType type, WGPUStringView message,
                    void* userdata1, void* userdata2) {
    (void)device;
    (void)type;
    (void)userdata1;
    (void)userdata2;
    if (message.data != nullptr && message.length > 0 && message.length != WGPU_STRLEN) {
        printf("[glim-web] uncaptured: %.*s\n", static_cast<int>(message.length), message.data);
    } else if (message.data != nullptr) {
        printf("[glim-web] uncaptured: %s\n", message.data);
    }
}

void requestDevice() {
    WebBootstrap& w = web();
    if (w.adapter == nullptr || w.deviceRequested || w.device != nullptr) {
        return;
    }
    WGPUDeviceDescriptor dd{};
    dd.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    dd.deviceLostCallbackInfo.callback = onDeviceLost;
    dd.uncapturedErrorCallbackInfo.callback = onUncaptured;
    WGPURequestDeviceCallbackInfo cb{};
    cb.mode = WGPUCallbackMode_AllowSpontaneous;
    cb.callback = onDevice;
    w.deviceRequested = true;
    wgpuAdapterRequestDevice(w.adapter, &dd, cb);
}

void onAdapter(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message,
               void* userdata1, void* userdata2) {
    (void)userdata1;
    (void)userdata2;
    WebBootstrap& w = web();
    if (status != WGPURequestAdapterStatus_Success || adapter == nullptr) {
        w.failed = true;
        w.error = "webgpu requestAdapter failed";
        if (message.data != nullptr && message.length != WGPU_STRLEN && message.length > 0) {
            w.error += ": ";
            w.error += std::string(message.data, message.length);
        }
        return;
    }
    w.adapter = adapter;
    requestDevice();
}

void ensureInstance(const char* selector) {
    WebBootstrap& w = web();
    if (w.instance != nullptr) {
        return;
    }
    if (selector != nullptr && selector[0] != '\0') {
        w.selector = selector;
    }
    w.instance = wgpuCreateInstance(nullptr);
    if (w.instance == nullptr) {
        w.failed = true;
        w.error = "wgpuCreateInstance failed";
        return;
    }
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector sel{};
    sel.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    sel.selector = strView(w.selector.c_str(), w.selector.size());
    WGPUSurfaceDescriptor sd{};
    sd.nextInChain = &sel.chain;
    w.surface = wgpuInstanceCreateSurface(w.instance, &sd);
    if (w.surface == nullptr) {
        w.failed = true;
        w.error = "webgpu surface creation failed";
    }
}

void ensureAdapter() {
    WebBootstrap& w = web();
    if (w.adapterRequested || w.surface == nullptr) {
        return;
    }
    WGPURequestAdapterOptions opts{};
    opts.compatibleSurface = w.surface;
    WGPURequestAdapterCallbackInfo cb{};
    cb.mode = WGPUCallbackMode_AllowSpontaneous;
    cb.callback = onAdapter;
    w.adapterRequested = true;
    wgpuInstanceRequestAdapter(w.instance, &opts, cb);
}

WGPUBuffer makeBuffer(WGPUDevice device, std::uint64_t size, WGPUBufferUsage usage) {
    WGPUBufferDescriptor bd{};
    bd.usage = usage;
    bd.size = size;
    return wgpuDeviceCreateBuffer(device, &bd);
}

WGPUTextureView makeView(WGPUTexture tex) {
    return wgpuTextureCreateView(tex, nullptr);
}

WGPUTextureView makeSingleMipView(WGPUTexture tex, WGPUTextureFormat format) {
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

}  // namespace

struct Device::Impl {
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
    WGPUSurface surface = nullptr;
    WGPUTextureFormat format = WGPUTextureFormat_Undefined;
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
    WGPUBuffer ring[kRingFrames] = {nullptr, nullptr, nullptr};
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
        td.usage = static_cast<WGPUTextureUsage>(WGPUTextureUsage_TextureBinding |
                                                 WGPUTextureUsage_CopyDst);
        td.dimension = WGPUTextureDimension_2D;
        td.size = {1, 1, 1};
        td.format = WGPUTextureFormat_RGBA8Unorm;
        td.mipLevelCount = 1;
        td.sampleCount = 1;
        dummyTex = wgpuDeviceCreateTexture(device, &td);
        if (dummyTex == nullptr) {
            return false;
        }
        dummyView = makeView(dummyTex);
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
        zeroBuf = makeBuffer(device, 64,
                             static_cast<WGPUBufferUsage>(WGPUBufferUsage_Storage |
                                                          WGPUBufferUsage_CopyDst));
        if (zeroBuf == nullptr) {
            return false;
        }
        for (WGPUBuffer& b : ring) {
            b = makeBuffer(device, kRingBytes,
                           static_cast<WGPUBufferUsage>(WGPUBufferUsage_Storage |
                                                        WGPUBufferUsage_CopyDst));
            if (b == nullptr) {
                return false;
            }
        }
        WGPUBindGroupLayoutEntry entries[8]{};
        for (int i = 0; i < 8; ++i) {
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
        for (int i = 2; i < 6; ++i) {
            entries[i].visibility = WGPUShaderStage_Fragment;
            entries[i].texture.sampleType = WGPUTextureSampleType_Float;
            entries[i].texture.viewDimension = WGPUTextureViewDimension_2D;
            entries[i].texture.multisampled = WGPU_FALSE;
        }
        for (int i = 6; i < 8; ++i) {
            entries[i].visibility = WGPUShaderStage_Fragment;
            entries[i].sampler.type = WGPUSamplerBindingType_Filtering;
        }
        WGPUBindGroupLayoutDescriptor bld{};
        bld.entryCount = 8;
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
        cfg.format = format;
        cfg.usage = static_cast<WGPUTextureUsage>(WGPUTextureUsage_RenderAttachment |
                                                  WGPUTextureUsage_TextureBinding |
                                                  WGPUTextureUsage_CopySrc);
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
        if (size > kRingBytes) {
            return false;
        }
        if (ring[ringIndex] == nullptr || ringOffset + size > kRingBytes) {
            ringIndex = (ringIndex + 1) % kRingFrames;
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
    WGPUTextureView tex[4] = {nullptr, nullptr, nullptr, nullptr};
    WGPUSampler samp[2] = {nullptr, nullptr};
};

struct CommandEncoder::Impl {
    Device* device = nullptr;
    WGPUCommandEncoder encoder = nullptr;
    bool presented = false;
};

Queue::Queue() = default;
Queue::Queue(void* native) : native_(native) {}

Pass::Pass() : impl_(new Impl()) {}
Pass::~Pass() {
    if (impl_ && impl_->pass && !impl_->ended) {
        wgpuRenderPassEncoderEnd(impl_->pass);
        wgpuRenderPassEncoderRelease(impl_->pass);
    }
}
Pass::Pass(Pass&&) noexcept = default;
Pass& Pass::operator=(Pass&&) noexcept = default;

void Pass::setPipeline(const Pipeline& pipeline) {
    if (impl_ && impl_->dev) {
        Handle h = pipeline.handle();
        if (h < impl_->dev->pipelines.size()) {
            impl_->pipeline = impl_->dev->pipelines[h].pipe;
        }
    }
}

void Pass::setBindings(std::uint32_t, const Bindings&) {}

void Pass::setVertexBuffer(std::uint32_t, const Buffer&, std::uint64_t) {}

void Pass::setBytes(std::uint32_t index, const void* data, std::uint64_t size) {
    if (!impl_ || !impl_->dev || !data || size == 0) {
        return;
    }
    Device::Impl* dev = impl_->dev;
    WGPUBuffer buf = nullptr;
    std::uint64_t off = 0;
    if (!dev->alloc(size, buf, off)) {
        return;
    }
    wgpuQueueWriteBuffer(dev->queue, buf, off, data, static_cast<size_t>(size));
    if (index == 0) {
        impl_->b0 = buf;
        impl_->b0off = off;
        impl_->b0size = size;
    } else if (index == 1) {
        impl_->b1 = buf;
        impl_->b1off = off;
        impl_->b1size = size;
    }
}

void Pass::setFragmentBytes(std::uint32_t index, const void* data, std::uint64_t size) {
    setBytes(index, data, size);
}

void Pass::setFragmentTexture(std::uint32_t index, void* nativeTexture) {
    if (impl_ && index < 4) {
        impl_->tex[index] = static_cast<WGPUTextureView>(nativeTexture);
    }
}

void Pass::setFragmentSampler(std::uint32_t index, void* nativeSampler) {
    if (impl_ && index < 2) {
        impl_->samp[index] = static_cast<WGPUSampler>(nativeSampler);
    }
}

void Pass::setViewport(float x, float y, float w, float h, float z0, float z1) {
    if (impl_ && impl_->pass) {
        wgpuRenderPassEncoderSetViewport(impl_->pass, x, y, w, h, z0, z1);
    }
}

void Pass::setScissor(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h) {
    if (impl_ && impl_->pass) {
        wgpuRenderPassEncoderSetScissorRect(impl_->pass, x, y, w, h);
    }
}

void Pass::draw(std::uint32_t vertexCount, std::uint32_t instanceCount, std::uint32_t firstVertex,
                std::uint32_t firstInstance) {
    if (!impl_ || !impl_->pass || !impl_->pipeline || !impl_->dev) {
        return;
    }
    Device::Impl* dev = impl_->dev;
    WGPUBuffer b0 = impl_->b0 != nullptr ? impl_->b0 : dev->zeroBuf;
    WGPUBuffer b1 = impl_->b1 != nullptr ? impl_->b1 : dev->zeroBuf;
    WGPUBindGroupEntry entries[8]{};
    entries[0].binding = 0;
    entries[0].buffer = b0;
    entries[0].offset = impl_->b0 != nullptr ? impl_->b0off : 0;
    entries[0].size = impl_->b0 != nullptr ? impl_->b0size : 64;
    entries[1].binding = 1;
    entries[1].buffer = b1;
    entries[1].offset = impl_->b1 != nullptr ? impl_->b1off : 0;
    entries[1].size = impl_->b1 != nullptr ? impl_->b1size : 64;
    for (int i = 0; i < 4; ++i) {
        entries[2 + i].binding = static_cast<std::uint32_t>(2 + i);
        entries[2 + i].textureView = impl_->tex[i] != nullptr ? impl_->tex[i] : dev->dummyView;
    }
    for (int i = 0; i < 2; ++i) {
        entries[6 + i].binding = static_cast<std::uint32_t>(6 + i);
        entries[6 + i].sampler = impl_->samp[i] != nullptr ? impl_->samp[i] : dev->sampler;
    }
    WGPUBindGroupDescriptor bgd{};
    bgd.layout = dev->bindLayout;
    bgd.entryCount = 8;
    bgd.entries = entries;
    WGPUBindGroup group = wgpuDeviceCreateBindGroup(dev->device, &bgd);
    if (group == nullptr) {
        return;
    }
    wgpuRenderPassEncoderSetPipeline(impl_->pass, impl_->pipeline);
    wgpuRenderPassEncoderSetBindGroup(impl_->pass, 0, group, 0, nullptr);
    wgpuRenderPassEncoderDraw(impl_->pass, vertexCount, instanceCount, firstVertex, firstInstance);
    wgpuBindGroupRelease(group);
}

void Pass::drawIndexed(std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t) {}

void Pass::end() {
    if (impl_ && impl_->pass && !impl_->ended) {
        wgpuRenderPassEncoderEnd(impl_->pass);
        wgpuRenderPassEncoderRelease(impl_->pass);
        impl_->pass = nullptr;
        impl_->ended = true;
    }
}

CommandEncoder::CommandEncoder() : impl_(new Impl()) {}
CommandEncoder::~CommandEncoder() = default;
CommandEncoder::CommandEncoder(CommandEncoder&&) noexcept = default;
CommandEncoder& CommandEncoder::operator=(CommandEncoder&&) noexcept = default;

Pass CommandEncoder::beginPass(const PassDesc& desc) {
    Pass pass;
    if (!impl_ || !impl_->device || !impl_->device->impl_ || !impl_->encoder) {
        return pass;
    }
    Device::Impl* dev = impl_->device->impl_.get();
    WGPUTextureView view = static_cast<WGPUTextureView>(desc.nativeColor);
    if (view == nullptr) {
        view = dev->swapView;
    }
    if (view == nullptr) {
        return pass;
    }
    WGPURenderPassColorAttachment color{};
    color.view = view;
    color.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    color.loadOp = desc.load == LoadOp::Load ? WGPULoadOp_Load : WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    color.clearValue = {desc.clear.rgba[0], desc.clear.rgba[1], desc.clear.rgba[2],
                        desc.clear.rgba[3]};
    WGPURenderPassDescriptor rpd{};
    rpd.colorAttachmentCount = 1;
    rpd.colorAttachments = &color;
    pass.impl_->dev = dev;
    pass.impl_->encoder = impl_->encoder;
    pass.impl_->pass = wgpuCommandEncoderBeginRenderPass(impl_->encoder, &rpd);
    if (desc.viewportW > 0 && desc.viewportH > 0) {
        pass.setViewport(0, 0, static_cast<float>(desc.viewportW),
                         static_cast<float>(desc.viewportH), 0, 1);
    }
    return pass;
}

bool CommandEncoder::copyColorTo(const FrameTarget&) {
    return false;
}

void CommandEncoder::generateMips(const FrameTarget&) {}

void CommandEncoder::present(const Drawable&) {
    if (impl_) {
        impl_->presented = true;
    }
}

void CommandEncoder::submit(Queue&) {
    if (!impl_ || !impl_->device || !impl_->device->impl_ || !impl_->encoder) {
        return;
    }
    Device::Impl* dev = impl_->device->impl_.get();
    WGPUCommandBuffer cmds = wgpuCommandEncoderFinish(impl_->encoder, nullptr);
    wgpuCommandEncoderRelease(impl_->encoder);
    impl_->encoder = nullptr;
    if (cmds != nullptr) {
        wgpuQueueSubmit(dev->queue, 1, &cmds);
        wgpuCommandBufferRelease(cmds);
    }
    if (impl_->presented) {
        impl_->presented = false;
    }
    dev->ringIndex = (dev->ringIndex + 1) % kRingFrames;
    dev->ringOffset = 0;
}

Result<Device> Device::create(const DeviceCreateInfo& info) {
    if (info.backend != Backend::WebGpu) {
        return Result<Device>::fail("glim-gpu-webgpu: DeviceCreateInfo.backend must be WebGpu");
    }
    WebBootstrap& w = web();
    if (w.failed) {
        return Result<Device>::fail(w.error.c_str());
    }
    const char* selector = static_cast<const char*>(info.native[0]);
    ensureInstance(selector);
    if (w.failed) {
        return Result<Device>::fail(w.error.c_str());
    }
    if (w.instance != nullptr) {
        wgpuInstanceProcessEvents(w.instance);
    }
    ensureAdapter();
    requestDevice();
    if (!w.ready || w.device == nullptr) {
        return Result<Device>::fail("webgpu device not ready");
    }
    Device out;
    out.impl_ = std::unique_ptr<Impl>(new Impl());
    Impl* dev = out.impl_.get();
    dev->device = w.device;
    dev->queue = w.queue;
    dev->surface = w.surface;
    dev->format = w.format;
    dev->queueWrapper = Queue(static_cast<void*>(w.queue));
    dev->shaders.resize(1);
    dev->pipelines.resize(1);
    dev->buffers.resize(1);
    dev->textures.resize(1);
    if (!dev->ensureCommon()) {
        return Result<Device>::fail("webgpu common resources failed");
    }
    return Result<Device>::ok(std::move(out));
}

Device::Device() = default;
Device::~Device() = default;
Device::Device(Device&&) noexcept = default;
Device& Device::operator=(Device&&) noexcept = default;

Queue& Device::queue() {
    return impl_->queueWrapper;
}

Result<Drawable> Device::nextDrawable() {
    WebBootstrap& w = web();
    if (!impl_ || !w.ready) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            printf("[glim-web] nextDrawable: not ready ready=%d device=%p\n", (int)w.ready,
                   (void*)w.device);
        }
        return Result<Drawable>::fail("webgpu device not ready");
    }
    wgpuInstanceProcessEvents(w.instance);
    if (!w.ready || w.device == nullptr) {
        return Result<Drawable>::fail("webgpu device lost");
    }
    impl_->rotateSwap();
    impl_->configure(impl_->drawableW, impl_->drawableH);
    WGPUSurfaceTexture st{};
    wgpuSurfaceGetCurrentTexture(impl_->surface, &st);
    const bool okStatus = st.status == WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal ||
                          st.status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal;
    if (!okStatus || st.texture == nullptr) {
        impl_->configuredW = 0;
        impl_->configuredH = 0;
        if (st.texture != nullptr) {
            wgpuTextureRelease(st.texture);
        }
        return Result<Drawable>::fail("webgpu current texture unavailable");
    }
    impl_->swapTex = st.texture;
    impl_->swapView = makeView(st.texture);
    if (impl_->swapView == nullptr) {
        impl_->releaseSwap();
        return Result<Drawable>::fail("webgpu texture view failed");
    }
    Drawable d;
    d.handle_ = 1;
    d.native_ = static_cast<void*>(impl_->swapView);
    d.width_ = impl_->drawableW;
    d.height_ = impl_->drawableH;
    return Result<Drawable>::ok(d);
}

void Device::setDrawableSize(int width, int height) {
    if (!impl_) {
        return;
    }
    if (width > 0) {
        impl_->drawableW = width;
    }
    if (height > 0) {
        impl_->drawableH = height;
    }
}

int Device::presentRotationDegrees() const {
    return 0;
}

Result<FrameTarget> Device::createFrameTarget(const FrameTargetDesc& desc) {
    const int w = std::max(desc.width, 1);
    const int h = std::max(desc.height, 1);
    std::uint32_t levels = 1;
    if (desc.mipmaps) {
        int m = std::max(w, h);
        while (m > 1) {
            m /= 2;
            ++levels;
        }
    }
    WGPUTextureDescriptor td{};
    td.usage = static_cast<WGPUTextureUsage>(WGPUTextureUsage_RenderAttachment |
                                             WGPUTextureUsage_TextureBinding);
    td.dimension = WGPUTextureDimension_2D;
    td.size = {static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 1};
    const WGPUTextureFormat rtFormat =
        impl_->format != WGPUTextureFormat_Undefined ? impl_->format : WGPUTextureFormat_RGBA8Unorm;
    td.format = rtFormat;
    td.mipLevelCount = levels;
    td.sampleCount = 1;
    WGPUTexture tex = wgpuDeviceCreateTexture(impl_->device, &td);
    if (tex == nullptr) {
        return Result<FrameTarget>::fail("webgpu FrameTarget texture failed");
    }
    WGPUTextureView view = makeSingleMipView(tex, rtFormat);
    if (view == nullptr) {
        wgpuTextureRelease(tex);
        return Result<FrameTarget>::fail("webgpu FrameTarget view failed");
    }
    FrameTarget t;
    t.handle_ = impl_->next++;
    t.width_ = w;
    t.height_ = h;
    t.native_ = static_cast<void*>(view);
    if (t.handle_ >= impl_->textures.size()) {
        impl_->textures.resize(t.handle_ + 1);
    }
    impl_->textures[t.handle_] = {tex, view, w, h};
    return Result<FrameTarget>::ok(t);
}

Result<Buffer> Device::createBuffer(const BufferDesc& desc) {
    WGPUBufferUsage usage = static_cast<WGPUBufferUsage>(WGPUBufferUsage_Storage |
                                                         WGPUBufferUsage_CopyDst);
    if (desc.vertex) {
        usage = static_cast<WGPUBufferUsage>(usage | WGPUBufferUsage_Vertex);
    }
    if (desc.index) {
        usage = static_cast<WGPUBufferUsage>(usage | WGPUBufferUsage_Index);
    }
    WGPUBuffer buf = makeBuffer(impl_->device, std::max<std::uint64_t>(desc.size, 16), usage);
    if (buf == nullptr) {
        return Result<Buffer>::fail("webgpu buffer failed");
    }
    Buffer b;
    b.handle_ = impl_->next++;
    if (b.handle_ >= impl_->buffers.size()) {
        impl_->buffers.resize(b.handle_ + 1);
    }
    impl_->buffers[b.handle_] = buf;
    b.native_ = static_cast<void*>(buf);
    b.size_ = std::max<std::uint64_t>(desc.size, 16);
    return Result<Buffer>::ok(b);
}

void Device::writeBuffer(Buffer& buffer, const void* data, std::uint64_t size) {
    if (!impl_ || !buffer.native() || !data) {
        return;
    }
    wgpuQueueWriteBuffer(impl_->queue, static_cast<WGPUBuffer>(buffer.native()), 0, data,
                         static_cast<size_t>(std::min<std::uint64_t>(size, buffer.size())));
}

Result<Texture> Device::createTexture(const TextureDesc& desc) {
    const int w = std::max(desc.width, 1);
    const int h = std::max(desc.height, 1);
    WGPUTextureDescriptor td{};
    td.usage = static_cast<WGPUTextureUsage>(WGPUTextureUsage_TextureBinding |
                                             WGPUTextureUsage_CopyDst);
    td.dimension = WGPUTextureDimension_2D;
    td.size = {static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    WGPUTexture tex = wgpuDeviceCreateTexture(impl_->device, &td);
    if (tex == nullptr) {
        return Result<Texture>::fail("webgpu texture failed");
    }
    WGPUTextureView view = makeView(tex);
    if (view == nullptr) {
        wgpuTextureRelease(tex);
        return Result<Texture>::fail("webgpu texture view failed");
    }
    Texture t;
    t.handle_ = impl_->next++;
    t.width_ = w;
    t.height_ = h;
    if (t.handle_ >= impl_->textures.size()) {
        impl_->textures.resize(t.handle_ + 1);
    }
    impl_->textures[t.handle_] = {tex, view, w, h};
    t.native_ = static_cast<void*>(view);
    return Result<Texture>::ok(t);
}

void Device::writeTexture(Texture& texture, const void* rgba8, std::uint64_t bytes) {
    if (!impl_ || !texture.native() || !rgba8 || texture.width() <= 0 || texture.height() <= 0) {
        return;
    }
    const std::uint64_t need =
        static_cast<std::uint64_t>(texture.width()) * 4 * static_cast<std::uint64_t>(texture.height());
    if (bytes < need) {
        return;
    }
    Handle h = texture.handle();
    if (h == 0 || h >= impl_->textures.size() || impl_->textures[h].tex == nullptr) {
        return;
    }
    const std::uint32_t w = static_cast<std::uint32_t>(texture.width());
    const std::uint32_t hh = static_cast<std::uint32_t>(texture.height());
    const std::uint32_t srcRow = w * 4;
    const std::uint32_t dstRow = (srcRow + 255u) & ~255u;
    WGPUTexelCopyTextureInfo dst{};
    dst.texture = impl_->textures[h].tex;
    dst.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferLayout layout{};
    layout.bytesPerRow = dstRow;
    layout.rowsPerImage = hh;
    WGPUExtent3D extent{w, hh, 1};
    if (dstRow == srcRow) {
        wgpuQueueWriteTexture(impl_->queue, &dst, rgba8, static_cast<size_t>(need), &layout,
                              &extent);
        return;
    }
    std::vector<std::uint8_t> padded(static_cast<std::size_t>(dstRow) * hh);
    const auto* src = static_cast<const std::uint8_t*>(rgba8);
    for (std::uint32_t y = 0; y < hh; ++y) {
        std::memcpy(padded.data() + static_cast<std::size_t>(y) * dstRow,
                    src + static_cast<std::size_t>(y) * srcRow, srcRow);
    }
    wgpuQueueWriteTexture(impl_->queue, &dst, padded.data(), padded.size(), &layout, &extent);
}

Result<Texture> Device::wrapNativeTexture(void* native, int width, int height) {
    if (!native || width <= 0 || height <= 0) {
        return Result<Texture>::fail("wrapNativeTexture: null or empty");
    }
    Texture t;
    t.handle_ = 0;
    t.width_ = width;
    t.height_ = height;
    t.native_ = native;
    return Result<Texture>::ok(t);
}

Result<Texture> Device::importSurface(void*, int, int, SampleFormat) {
    return Result<Texture>::fail("webgpu importSurface unsupported");
}

Result<Shader> Device::createShader(ShaderStage, const char* sourceUtf8, std::uint64_t size) {
    if (!sourceUtf8 || size == 0) {
        return Result<Shader>::fail("empty shader source");
    }
    WGPUShaderSourceWGSL wgsl{};
    wgsl.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl.code = strView(sourceUtf8, static_cast<std::size_t>(size));
    WGPUShaderModuleDescriptor md{};
    md.nextInChain = &wgsl.chain;
    WGPUShaderModule mod = wgpuDeviceCreateShaderModule(impl_->device, &md);
    if (mod == nullptr) {
        return Result<Shader>::fail("webgpu shader module failed");
    }
    Shader s;
    s.handle_ = impl_->next++;
    if (s.handle_ >= impl_->shaders.size()) {
        impl_->shaders.resize(s.handle_ + 1);
    }
    impl_->shaders[s.handle_] = mod;
    s.native_ = static_cast<void*>(mod);
    return Result<Shader>::ok(s);
}

Result<Pipeline> Device::createPipeline(const PipelineDesc& desc) {
    if (!impl_->ensureCommon()) {
        return Result<Pipeline>::fail("webgpu common resources failed");
    }
    if (desc.vertexShader >= impl_->shaders.size() || desc.fragmentShader >= impl_->shaders.size() ||
        !impl_->shaders[desc.vertexShader] || !impl_->shaders[desc.fragmentShader]) {
        return Result<Pipeline>::fail("invalid shader handles");
    }
    WGPUBlendComponent comp{};
    comp.operation = WGPUBlendOperation_Add;
    comp.srcFactor = WGPUBlendFactor_One;
    comp.dstFactor = desc.blend == Blend::Plus ? WGPUBlendFactor_One
                                               : WGPUBlendFactor_OneMinusSrcAlpha;
    WGPUBlendState blend{};
    blend.color = comp;
    blend.alpha = comp;
    WGPUColorTargetState target{};
    target.format = impl_->format;
    target.blend = &blend;
    target.writeMask = WGPUColorWriteMask_All;
    WGPUFragmentState frag{};
    frag.module = impl_->shaders[desc.fragmentShader];
    frag.entryPoint = strView("fs_main", 7);
    frag.targetCount = 1;
    frag.targets = &target;
    WGPUVertexState vert{};
    vert.module = impl_->shaders[desc.vertexShader];
    vert.entryPoint = strView("vs_main", 7);
    WGPUPrimitiveState prim{};
    prim.topology = WGPUPrimitiveTopology_TriangleList;
    prim.frontFace = WGPUFrontFace_CCW;
    prim.cullMode = WGPUCullMode_None;
    WGPUMultisampleState ms{};
    ms.count = 1;
    ms.mask = 0xFFFFFFFF;
    WGPURenderPipelineDescriptor pd{};
    pd.layout = impl_->pipeLayout;
    pd.vertex = vert;
    pd.primitive = prim;
    pd.fragment = &frag;
    pd.multisample = ms;
    WGPURenderPipeline pipe = wgpuDeviceCreateRenderPipeline(impl_->device, &pd);
    if (pipe == nullptr) {
        return Result<Pipeline>::fail("webgpu pipeline failed");
    }
    Pipeline p;
    p.handle_ = impl_->next++;
    if (p.handle_ >= impl_->pipelines.size()) {
        impl_->pipelines.resize(p.handle_ + 1);
    }
    impl_->pipelines[p.handle_].pipe = pipe;
    p.native_ = static_cast<void*>(pipe);
    return Result<Pipeline>::ok(p);
}

Result<Bindings> Device::createBindings(const BindingsDesc&) {
    Bindings b;
    b.handle_ = impl_->next++;
    return Result<Bindings>::ok(b);
}

CommandEncoder Device::encoder() {
    CommandEncoder enc;
    if (!impl_) {
        return enc;
    }
    enc.impl_->device = this;
    enc.impl_->encoder = wgpuDeviceCreateCommandEncoder(impl_->device, nullptr);
    return enc;
}

void* Device::nativeDevice() const {
    return impl_ ? static_cast<void*>(impl_->device) : nullptr;
}

void* Device::nativeLayer() const {
    return impl_ ? static_cast<void*>(impl_->surface) : nullptr;
}

void* Device::nativeSampler() const {
    return impl_ ? static_cast<void*>(impl_->sampler) : nullptr;
}

void* Device::colorNative() const {
    return impl_ ? static_cast<void*>(impl_->swapView) : nullptr;
}

}  // namespace glim::gpu

#else

namespace glim::gpu {

Queue::Queue() = default;
Queue::Queue(void* native) : native_(native) {}

struct Pass::Impl {
    bool open = false;
};

Pass::Pass() : impl_(new Impl()) {}
Pass::~Pass() = default;
Pass::Pass(Pass&&) noexcept = default;
Pass& Pass::operator=(Pass&&) noexcept = default;

void Pass::setPipeline(const Pipeline&) {}
void Pass::setBindings(std::uint32_t, const Bindings&) {}
void Pass::setVertexBuffer(std::uint32_t, const Buffer&, std::uint64_t) {}
void Pass::setBytes(std::uint32_t, const void*, std::uint64_t) {}
void Pass::setFragmentBytes(std::uint32_t, const void*, std::uint64_t) {}
void Pass::setFragmentTexture(std::uint32_t, void*) {}
void Pass::setFragmentSampler(std::uint32_t, void*) {}
void Pass::setViewport(float, float, float, float, float, float) {}
void Pass::setScissor(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {}
void Pass::draw(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {}
void Pass::drawIndexed(std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t) {}
void Pass::end() {}

struct CommandEncoder::Impl {
    bool open = false;
};

CommandEncoder::CommandEncoder() : impl_(new Impl()) {}
CommandEncoder::~CommandEncoder() = default;
CommandEncoder::CommandEncoder(CommandEncoder&&) noexcept = default;
CommandEncoder& CommandEncoder::operator=(CommandEncoder&&) noexcept = default;

Pass CommandEncoder::beginPass(const PassDesc&) {
    return Pass();
}
bool CommandEncoder::copyColorTo(const FrameTarget&) {
    return false;
}
void CommandEncoder::generateMips(const FrameTarget&) {}
void CommandEncoder::present(const Drawable&) {}
void CommandEncoder::submit(Queue&) {}

struct Device::Impl {
    DeviceCreateInfo info{};
    Queue queue{};
    int drawableW = 1;
    int drawableH = 1;
};

Result<Device> Device::create(const DeviceCreateInfo& info) {
    Device device;
    device.impl_ = std::unique_ptr<Impl>(new Impl());
    device.impl_->info = info;
    if (info.backend != Backend::WebGpu) {
        return Result<Device>::fail("glim-gpu-webgpu: DeviceCreateInfo.backend must be WebGpu");
    }
    return Result<Device>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}

Device::Device() = default;
Device::~Device() = default;
Device::Device(Device&&) noexcept = default;
Device& Device::operator=(Device&&) noexcept = default;

Queue& Device::queue() {
    return impl_->queue;
}
Result<Drawable> Device::nextDrawable() {
    return Result<Drawable>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
void Device::setDrawableSize(int width, int height) {
    if (!impl_) {
        return;
    }
    if (width > 0) {
        impl_->drawableW = width;
    }
    if (height > 0) {
        impl_->drawableH = height;
    }
}
int Device::presentRotationDegrees() const {
    return 0;
}
Result<FrameTarget> Device::createFrameTarget(const FrameTargetDesc&) {
    return Result<FrameTarget>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
Result<Buffer> Device::createBuffer(const BufferDesc&) {
    return Result<Buffer>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
void Device::writeBuffer(Buffer&, const void*, std::uint64_t) {}
Result<Texture> Device::createTexture(const TextureDesc&) {
    return Result<Texture>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
void Device::writeTexture(Texture&, const void*, std::uint64_t) {}
Result<Texture> Device::wrapNativeTexture(void*, int, int) {
    return Result<Texture>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
Result<Texture> Device::importSurface(void*, int, int, SampleFormat) {
    return Result<Texture>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
Result<Shader> Device::createShader(ShaderStage, const char*, std::uint64_t) {
    return Result<Shader>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
Result<Pipeline> Device::createPipeline(const PipelineDesc&) {
    return Result<Pipeline>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
Result<Bindings> Device::createBindings(const BindingsDesc&) {
    return Result<Bindings>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
CommandEncoder Device::encoder() {
    return CommandEncoder();
}

void* Device::nativeDevice() const {
    return nullptr;
}
void* Device::nativeLayer() const {
    return nullptr;
}
void* Device::nativeSampler() const {
    return nullptr;
}
void* Device::colorNative() const {
    return nullptr;
}

}  // namespace glim::gpu

#endif
