#include <glim/gpu/Device.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <cstring>
#include <vector>

namespace glim::gpu {
namespace {

NSString* functionNameForStage(ShaderStage stage) {
    return stage == ShaderStage::Vertex ? @"vs_main" : @"fs_main";
}

MTLBlendFactor dstFactor(Blend blend) {
    return blend == Blend::Plus ? MTLBlendFactorOne : MTLBlendFactorOneMinusSourceAlpha;
}

}  // namespace

struct Device::Impl {
    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    CAMetalLayer* layer = nil;
    id<CAMetalDrawable> currentDrawable = nil;
    Queue queueWrapper;
    Handle next = 1;
    std::vector<id<MTLFunction>> shaders;
    std::vector<id<MTLRenderPipelineState>> pipelines;
    std::vector<id<MTLBuffer>> buffers;
};

struct Pass::Impl {
    id<MTLRenderCommandEncoder> encoder = nil;
    bool ended = false;
};

struct CommandEncoder::Impl {
    Device* device = nullptr;
    id<MTLCommandBuffer> commandBuffer = nil;
    id<CAMetalDrawable> drawable = nil;
};

Queue::Queue() = default;
Queue::Queue(void* native) : native_(native) {}

Pass::Pass() : impl_(std::make_unique<Impl>()) {}
Pass::~Pass() {
    if (impl_ && impl_->encoder && !impl_->ended) {
        [impl_->encoder endEncoding];
    }
}
Pass::Pass(Pass&&) noexcept = default;
Pass& Pass::operator=(Pass&&) noexcept = default;

void Pass::setPipeline(const Pipeline& pipeline) {
    [impl_->encoder setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)pipeline.native()];
}

void Pass::setBindings(std::uint32_t, const Bindings&) {}

void Pass::setVertexBuffer(std::uint32_t index, const Buffer& buffer, std::uint64_t offset) {
    [impl_->encoder setVertexBuffer:(__bridge id<MTLBuffer>)buffer.native() offset:offset atIndex:index];
}

void Pass::setBytes(std::uint32_t index, const void* data, std::uint64_t size) {
    [impl_->encoder setVertexBytes:data length:static_cast<NSUInteger>(size) atIndex:index];
}

void Pass::setViewport(float x, float y, float w, float h, float z0, float z1) {
    MTLViewport vp{x, y, w, h, z0, z1};
    [impl_->encoder setViewport:vp];
}

void Pass::setScissor(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h) {
    MTLScissorRect r{x, y, w, h};
    [impl_->encoder setScissorRect:r];
}

void Pass::draw(std::uint32_t vertexCount, std::uint32_t instanceCount, std::uint32_t firstVertex,
                std::uint32_t firstInstance) {
    [impl_->encoder drawPrimitives:MTLPrimitiveTypeTriangle
                       vertexStart:firstVertex
                       vertexCount:vertexCount
                     instanceCount:instanceCount
                      baseInstance:firstInstance];
}

void Pass::drawIndexed(std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t) {}

void Pass::end() {
    if (impl_ && impl_->encoder && !impl_->ended) {
        [impl_->encoder endEncoding];
        impl_->ended = true;
    }
}

CommandEncoder::CommandEncoder() : impl_(std::make_unique<Impl>()) {}
CommandEncoder::~CommandEncoder() = default;
CommandEncoder::CommandEncoder(CommandEncoder&&) noexcept = default;
CommandEncoder& CommandEncoder::operator=(CommandEncoder&&) noexcept = default;

Pass CommandEncoder::beginPass(const PassDesc& desc) {
    Pass pass;
    MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
    rpd.colorAttachments[0].texture = impl_->drawable.texture;
    switch (desc.load) {
        case LoadOp::Load:
            rpd.colorAttachments[0].loadAction = MTLLoadActionLoad;
            break;
        case LoadOp::DontCare:
            rpd.colorAttachments[0].loadAction = MTLLoadActionDontCare;
            break;
        case LoadOp::Clear:
        default:
            rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
            rpd.colorAttachments[0].clearColor = MTLClearColorMake(
                desc.clear.rgba[0], desc.clear.rgba[1], desc.clear.rgba[2], desc.clear.rgba[3]);
            break;
    }
    rpd.colorAttachments[0].storeAction =
        desc.store == StoreOp::DontCare ? MTLStoreActionDontCare : MTLStoreActionStore;
    pass.impl_->encoder = [impl_->commandBuffer renderCommandEncoderWithDescriptor:rpd];
    if (desc.viewportW > 0 && desc.viewportH > 0) {
        pass.setViewport(0, 0, static_cast<float>(desc.viewportW), static_cast<float>(desc.viewportH), 0, 1);
    }
    return pass;
}

void CommandEncoder::present(const Drawable& drawable) {
    id<CAMetalDrawable> d = (__bridge id<CAMetalDrawable>)drawable.native();
    if (d) {
        [impl_->commandBuffer presentDrawable:d];
    }
}

void CommandEncoder::submit(Queue&) {
    [impl_->commandBuffer commit];
}

Device::Device() = default;
Device::~Device() = default;
Device::Device(Device&&) noexcept = default;
Device& Device::operator=(Device&&) noexcept = default;

Result<Device> Device::create(const DeviceCreateInfo& info) {
    if (info.backend != Backend::Metal) {
        return Result<Device>::fail("glim-gpu-metal: backend is not Metal");
    }
    CAMetalLayer* layer = (__bridge CAMetalLayer*)info.native[0];
    if (!layer) {
        return Result<Device>::fail("Metal layer is null");
    }
    id<MTLDevice> device = (__bridge id<MTLDevice>)info.native[1];
    if (!device) {
        device = layer.device ? layer.device : MTLCreateSystemDefaultDevice();
    }
    if (!device) {
        return Result<Device>::fail("No Metal device");
    }
    layer.device = device;

    Device out;
    out.impl_ = std::make_unique<Impl>();
    out.impl_->device = device;
    out.impl_->commandQueue = [device newCommandQueue];
    out.impl_->layer = layer;
    out.impl_->queueWrapper = Queue((__bridge void*)out.impl_->commandQueue);
    out.impl_->shaders.resize(1);
    out.impl_->pipelines.resize(1);
    out.impl_->buffers.resize(1);
    return Result<Device>::ok(std::move(out));
}

Queue& Device::queue() {
    return impl_->queueWrapper;
}

Result<Drawable> Device::nextDrawable() {
    impl_->currentDrawable = [impl_->layer nextDrawable];
    if (!impl_->currentDrawable) {
        return Result<Drawable>::fail("nextDrawable returned nil");
    }
    Drawable d;
    d.handle_ = 1;
    d.native_ = (__bridge void*)impl_->currentDrawable;
    d.width_ = static_cast<int>(impl_->layer.drawableSize.width);
    d.height_ = static_cast<int>(impl_->layer.drawableSize.height);
    return Result<Drawable>::ok(d);
}

Result<FrameTarget> Device::createFrameTarget(const FrameTargetDesc& desc) {
    FrameTarget t;
    t.handle_ = impl_->next++;
    t.width_ = desc.width;
    t.height_ = desc.height;
    return Result<FrameTarget>::ok(t);
}

Result<Buffer> Device::createBuffer(const BufferDesc& desc) {
    const NSUInteger size = static_cast<NSUInteger>(std::max<std::uint64_t>(desc.size, 16));
    id<MTLBuffer> buf = [impl_->device newBufferWithLength:size options:MTLResourceStorageModeShared];
    if (!buf) {
        return Result<Buffer>::fail("newBufferWithLength failed");
    }
    Buffer b;
    b.handle_ = impl_->next++;
    if (b.handle_ >= impl_->buffers.size()) {
        impl_->buffers.resize(b.handle_ + 1);
    }
    impl_->buffers[b.handle_] = buf;
    b.native_ = (__bridge void*)buf;
    b.size_ = size;
    return Result<Buffer>::ok(b);
}

void Device::writeBuffer(Buffer& buffer, const void* data, std::uint64_t size) {
    id<MTLBuffer> buf = (__bridge id<MTLBuffer>)buffer.native();
    if (!buf || !data) {
        return;
    }
    std::memcpy(buf.contents, data, static_cast<size_t>(std::min<std::uint64_t>(size, buffer.size())));
}

Result<Texture> Device::createTexture(const TextureDesc&) {
    Texture t;
    t.handle_ = impl_->next++;
    return Result<Texture>::ok(t);
}

Result<Shader> Device::createShader(ShaderStage stage, const char* sourceUtf8, std::uint64_t size) {
    if (!sourceUtf8 || size == 0) {
        return Result<Shader>::fail("empty shader source");
    }
    NSString* src = [[NSString alloc] initWithBytes:sourceUtf8
                                             length:static_cast<NSUInteger>(size)
                                           encoding:NSUTF8StringEncoding];
    NSError* error = nil;
    id<MTLLibrary> lib = [impl_->device newLibraryWithSource:src options:nil error:&error];
    if (!lib) {
        const char* msg = error.localizedDescription.UTF8String;
        return Result<Shader>::fail(msg ? std::string(msg) : std::string("Metal shader compile failed"));
    }
    id<MTLFunction> fn = [lib newFunctionWithName:functionNameForStage(stage)];
    if (!fn) {
        return Result<Shader>::fail("Metal function not found");
    }
    Shader s;
    s.handle_ = impl_->next++;
    if (s.handle_ >= impl_->shaders.size()) {
        impl_->shaders.resize(s.handle_ + 1);
    }
    impl_->shaders[s.handle_] = fn;
    s.native_ = (__bridge void*)fn;
    return Result<Shader>::ok(s);
}

Result<Pipeline> Device::createPipeline(const PipelineDesc& desc) {
    if (desc.vertexShader >= impl_->shaders.size() || desc.fragmentShader >= impl_->shaders.size() ||
        !impl_->shaders[desc.vertexShader] || !impl_->shaders[desc.fragmentShader]) {
        return Result<Pipeline>::fail("invalid shader handles");
    }
    MTLRenderPipelineDescriptor* pd = [[MTLRenderPipelineDescriptor alloc] init];
    pd.vertexFunction = impl_->shaders[desc.vertexShader];
    pd.fragmentFunction = impl_->shaders[desc.fragmentShader];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    pd.colorAttachments[0].blendingEnabled = YES;
    pd.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
    pd.colorAttachments[0].destinationRGBBlendFactor = dstFactor(desc.blend);
    pd.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pd.colorAttachments[0].destinationAlphaBlendFactor = dstFactor(desc.blend);
    pd.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pd.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;

    NSError* error = nil;
    id<MTLRenderPipelineState> pso = [impl_->device newRenderPipelineStateWithDescriptor:pd error:&error];
    if (!pso) {
        const char* msg = error.localizedDescription.UTF8String;
        return Result<Pipeline>::fail(msg ? std::string(msg) : std::string("pipeline create failed"));
    }
    Pipeline p;
    p.handle_ = impl_->next++;
    if (p.handle_ >= impl_->pipelines.size()) {
        impl_->pipelines.resize(p.handle_ + 1);
    }
    impl_->pipelines[p.handle_] = pso;
    p.native_ = (__bridge void*)pso;
    return Result<Pipeline>::ok(p);
}

Result<Bindings> Device::createBindings(const BindingsDesc&) {
    Bindings b;
    b.handle_ = impl_->next++;
    return Result<Bindings>::ok(b);
}

CommandEncoder Device::encoder() {
    CommandEncoder enc;
    enc.impl_->device = this;
    enc.impl_->commandBuffer = [impl_->commandQueue commandBuffer];
    enc.impl_->drawable = impl_->currentDrawable;
    return enc;
}

void* Device::nativeDevice() const {
    return impl_ ? (__bridge void*)impl_->device : nullptr;
}

void* Device::nativeLayer() const {
    return impl_ ? (__bridge void*)impl_->layer : nullptr;
}

}  // namespace glim::gpu
