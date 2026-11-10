#include <glim/gpu/Device.h>

#import <IOSurface/IOSurfaceRef.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
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
  // Shared with completion handlers, which may outlive the device.
  std::shared_ptr<std::atomic<float>> gpuMs = std::make_shared<std::atomic<float>>(0.f);
  Queue queueWrapper;
  Handle next = 1;
  std::vector<id<MTLFunction>> shaders;
  std::vector<id<MTLRenderPipelineState>> pipelines;
  std::vector<id<MTLBuffer>> buffers;
  std::vector<id<MTLTexture>> textures;
  struct PooledTarget {
    id<MTLTexture> tex = nil;
    bool inUse = false;
    bool mipmaps = false;
  };
  std::vector<PooledTarget> targetPool;
  id<MTLSamplerState> sampler = nil;
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
  [impl_->encoder setVertexBuffer:(__bridge id<MTLBuffer>)buffer.native()
                           offset:offset
                          atIndex:index];
}

void Pass::setBytes(std::uint32_t index, const void* data, std::uint64_t size) {
  [impl_->encoder setVertexBytes:data length:static_cast<NSUInteger>(size) atIndex:index];
}

void Pass::setFragmentBytes(std::uint32_t index, const void* data, std::uint64_t size) {
  [impl_->encoder setFragmentBytes:data length:static_cast<NSUInteger>(size) atIndex:index];
}

void Pass::setFragmentTexture(std::uint32_t index, void* nativeTexture) {
  id<MTLTexture> tex = (__bridge id<MTLTexture>)nativeTexture;
  [impl_->encoder setFragmentTexture:tex atIndex:index];
}

void Pass::setFragmentSampler(std::uint32_t index, void* nativeSampler) {
  id<MTLSamplerState> samp = (__bridge id<MTLSamplerState>)nativeSampler;
  [impl_->encoder setFragmentSamplerState:samp atIndex:index];
}

void Pass::setViewport(float x, float y, float w, float h, float z0, float z1) {
  MTLViewport vp{x, y, w, h, z0, z1};
  [impl_->encoder setViewport:vp];
}

void Pass::setScissor(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h) {
  MTLScissorRect r{x, y, w, h};
  [impl_->encoder setScissorRect:r];
}

void Pass::draw(
    std::uint32_t vertexCount,
    std::uint32_t instanceCount,
    std::uint32_t firstVertex,
    std::uint32_t firstInstance
) {
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
  if (desc.nativeColor) {
    rpd.colorAttachments[0].texture = (__bridge id<MTLTexture>)desc.nativeColor;
  } else {
    rpd.colorAttachments[0].texture = impl_->drawable.texture;
  }
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
          desc.clear.rgba[0], desc.clear.rgba[1], desc.clear.rgba[2], desc.clear.rgba[3]
      );
      break;
  }
  rpd.colorAttachments[0].storeAction =
      desc.store == StoreOp::DontCare ? MTLStoreActionDontCare : MTLStoreActionStore;
  pass.impl_->encoder = [impl_->commandBuffer renderCommandEncoderWithDescriptor:rpd];
  if (desc.viewportW > 0 && desc.viewportH > 0) {
    pass.setViewport(
        0, 0, static_cast<float>(desc.viewportW), static_cast<float>(desc.viewportH), 0, 1
    );
  }
  return pass;
}

bool CommandEncoder::copyColorTo(const FrameTarget&) { return false; }

void CommandEncoder::generateMips(const FrameTarget& dst) {
  id<MTLTexture> tex = (__bridge id<MTLTexture>)dst.native();
  if (!impl_->commandBuffer || !tex || tex.mipmapLevelCount <= 1) {
    return;
  }
  id<MTLBlitCommandEncoder> blit = [impl_->commandBuffer blitCommandEncoder];
  [blit generateMipmapsForTexture:tex];
  [blit endEncoding];
}

void CommandEncoder::present(const Drawable& drawable) {
  impl_->drawable = (__bridge id<CAMetalDrawable>)drawable.native();
}

void CommandEncoder::submit(Queue&) {
  Device::Impl* device = impl_->device ? impl_->device->impl_.get() : nullptr;
  const bool synced = device && device->layer && device->layer.presentsWithTransaction;
  if (device) {
    std::shared_ptr<std::atomic<float>> gpuMs = device->gpuMs;
    [impl_->commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> cb) {
      gpuMs->store(static_cast<float>((cb.GPUEndTime - cb.GPUStartTime) * 1000.0));
    }];
  }
  if (synced && impl_->drawable) {
    [impl_->commandBuffer commit];
    [impl_->commandBuffer waitUntilScheduled];
    [impl_->drawable present];
  } else {
    if (impl_->drawable) {
      [impl_->commandBuffer presentDrawable:impl_->drawable];
    }
    [impl_->commandBuffer commit];
  }
  // A presented drawable held past submit pins one of the layer's few
  // slots (nextDrawable then waits on the display, up to its 1 s
  // timeout) and would be presented again by the next encoder.
  if (impl_->drawable && device && device->currentDrawable == impl_->drawable) {
    device->currentDrawable = nil;
  }
  impl_->drawable = nil;
}

Device::Device() = default;
Device::~Device() = default;
Device::Device(Device&&) noexcept = default;
Device& Device::operator=(Device&&) noexcept = default;

Result<Device> Device::create(const DeviceCreateInfo& info) {
  if (info.backend != Backend::Metal) {
    return Result<Device>::fail("glim-gpu-metal: backend is not Metal");
  }
  // Null layer = headless (FrameTargets + readPixels only), same contract as Vulkan.
  CAMetalLayer* layer = (__bridge CAMetalLayer*)info.native[0];
  id<MTLDevice> device = (__bridge id<MTLDevice>)info.native[1];
  if (!device) {
    device = layer && layer.device ? layer.device : MTLCreateSystemDefaultDevice();
  }
  if (!device) {
    return Result<Device>::fail("No Metal device");
  }
  if (layer) {
    layer.device = device;
  }

  Device out;
  out.impl_ = std::make_unique<Impl>();
  out.impl_->device = device;
  out.impl_->commandQueue = [device newCommandQueue];
  out.impl_->layer = layer;
  out.impl_->queueWrapper = Queue((__bridge void*)out.impl_->commandQueue);
  out.impl_->shaders.resize(1);
  out.impl_->pipelines.resize(1);
  out.impl_->buffers.resize(1);
  out.impl_->textures.resize(1);
  MTLSamplerDescriptor* samp = [[MTLSamplerDescriptor alloc] init];
  samp.minFilter = MTLSamplerMinMagFilterLinear;
  samp.magFilter = MTLSamplerMinMagFilterLinear;
  samp.mipFilter = MTLSamplerMipFilterLinear;
  samp.sAddressMode = MTLSamplerAddressModeClampToEdge;
  samp.tAddressMode = MTLSamplerAddressModeClampToEdge;
  out.impl_->sampler = [device newSamplerStateWithDescriptor:samp];
  return Result<Device>::ok(std::move(out));
}

Queue& Device::queue() { return impl_->queueWrapper; }

void Device::setDrawableSize(int width, int height) {
  if (!impl_ || !impl_->layer || width <= 0 || height <= 0) {
    return;
  }
  // Join the in-flight implicit transaction; drawableSize applies immediately,
  // so no flush or per-paint commit that desyncs the layer mid-drag.
  impl_->layer.drawableSize = CGSizeMake(static_cast<CGFloat>(width), static_cast<CGFloat>(height));
}

Extent Device::drawableSize() const {
  if (!impl_ || !impl_->layer) {
    return {};
  }
  const CGSize s = impl_->layer.drawableSize;
  return {static_cast<int>(s.width), static_cast<int>(s.height)};
}

int Device::presentRotationDegrees() const { return 0; }

float Device::lastGpuMs() const { return impl_ ? impl_->gpuMs->load() : 0.f; }

Result<Drawable> Device::nextDrawable() {
  for (Impl::PooledTarget& t : impl_->targetPool) {
    t.inUse = false;
  }
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
  const int w = std::max(desc.width, 1);
  const int h = std::max(desc.height, 1);
  const bool mips = desc.mipmaps;
  id<MTLTexture> tex = nil;
  for (Impl::PooledTarget& slot : impl_->targetPool) {
    if (!slot.inUse && slot.tex && slot.mipmaps == mips && static_cast<int>(slot.tex.width) == w &&
        static_cast<int>(slot.tex.height) == h) {
      slot.inUse = true;
      tex = slot.tex;
      break;
    }
  }
  if (!tex) {
    // sRGB offscreen matches swapchain; single-format rule (see Surface::attach).
    MTLTextureDescriptor* td =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm_sRGB
                                                           width:static_cast<NSUInteger>(w)
                                                          height:static_cast<NSUInteger>(h)
                                                       mipmapped:mips ? YES : NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModePrivate;
    tex = [impl_->device newTextureWithDescriptor:td];
    if (!tex) {
      return Result<FrameTarget>::fail("Metal FrameTarget texture failed");
    }
    impl_->targetPool.push_back({tex, true, mips});
  }
  FrameTarget t;
  t.handle_ = impl_->next++;
  t.width_ = w;
  t.height_ = h;
  t.native_ = (__bridge void*)tex;
  return Result<FrameTarget>::ok(t);
}

bool Device::headless() const { return impl_ && !impl_->layer; }

bool Device::readPixels(const FrameTarget& target, void* rgba8, std::uint64_t bytes) {
  id<MTLTexture> tex = (__bridge id<MTLTexture>)target.native();
  const NSUInteger w = static_cast<NSUInteger>(target.width());
  const NSUInteger h = static_cast<NSUInteger>(target.height());
  const NSUInteger rowBytes = w * 4;
  if (!impl_ || !tex || !rgba8 || bytes < rowBytes * h) {
    return false;
  }
  id<MTLBuffer> staging = [impl_->device newBufferWithLength:rowBytes * h
                                                     options:MTLResourceStorageModeShared];
  // Same queue: ordered after every frame already committed to the target.
  id<MTLCommandBuffer> cb = [impl_->commandQueue commandBuffer];
  id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
  [blit copyFromTexture:tex
                   sourceSlice:0
                   sourceLevel:0
                  sourceOrigin:MTLOriginMake(0, 0, 0)
                    sourceSize:MTLSizeMake(w, h, 1)
                      toBuffer:staging
             destinationOffset:0
        destinationBytesPerRow:rowBytes
      destinationBytesPerImage:rowBytes * h];
  [blit endEncoding];
  [cb commit];
  [cb waitUntilCompleted];
  if (cb.status != MTLCommandBufferStatusCompleted) {
    return false;
  }
  // BGRA8 sRGB bytes -> RGBA8 sRGB bytes (no transfer-function change).
  const auto* src = static_cast<const std::uint8_t*>(staging.contents);
  auto* dst = static_cast<std::uint8_t*>(rgba8);
  for (NSUInteger i = 0; i < w * h; ++i) {
    dst[i * 4 + 0] = src[i * 4 + 2];
    dst[i * 4 + 1] = src[i * 4 + 1];
    dst[i * 4 + 2] = src[i * 4 + 0];
    dst[i * 4 + 3] = src[i * 4 + 3];
  }
  return true;
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
  std::memcpy(
      buf.contents, data, static_cast<size_t>(std::min<std::uint64_t>(size, buffer.size()))
  );
}

Result<Texture> Device::createTexture(const TextureDesc& desc) {
  const int w = std::max(desc.width, 1);
  const int h = std::max(desc.height, 1);
  MTLTextureDescriptor* td =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                         width:static_cast<NSUInteger>(w)
                                                        height:static_cast<NSUInteger>(h)
                                                     mipmapped:NO];
  td.usage = MTLTextureUsageShaderRead;
  td.storageMode = MTLStorageModeShared;
  id<MTLTexture> tex = [impl_->device newTextureWithDescriptor:td];
  if (!tex) {
    return Result<Texture>::fail("Metal texture failed");
  }
  Texture t;
  t.handle_ = impl_->next++;
  t.width_ = w;
  t.height_ = h;
  if (t.handle_ >= impl_->textures.size()) {
    impl_->textures.resize(t.handle_ + 1);
  }
  impl_->textures[t.handle_] = tex;
  t.native_ = (__bridge void*)tex;
  return Result<Texture>::ok(t);
}

void Device::writeTexture(Texture& texture, const void* rgba8, std::uint64_t bytes) {
  id<MTLTexture> tex = (__bridge id<MTLTexture>)texture.native();
  if (!tex || !rgba8 || texture.width() <= 0 || texture.height() <= 0) {
    return;
  }
  const NSUInteger bpr = static_cast<NSUInteger>(texture.width()) * 4;
  const std::uint64_t need =
      static_cast<std::uint64_t>(bpr) * static_cast<std::uint64_t>(texture.height());
  if (bytes < need) {
    return;
  }
  MTLRegion region = MTLRegionMake2D(
      0, 0, static_cast<NSUInteger>(texture.width()), static_cast<NSUInteger>(texture.height())
  );
  [tex replaceRegion:region mipmapLevel:0 withBytes:rgba8 bytesPerRow:bpr];
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

Result<Texture> Device::importSurface(void* surface, int width, int height, SampleFormat format) {
  if (!impl_ || !surface || width <= 0 || height <= 0) {
    return Result<Texture>::fail("importSurface: null or empty");
  }
  IOSurfaceRef surf = static_cast<IOSurfaceRef>(surface);
  const MTLPixelFormat pf =
      format == SampleFormat::Bgra8Unorm ? MTLPixelFormatBGRA8Unorm : MTLPixelFormatRGBA8Unorm;
  MTLTextureDescriptor* td =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pf
                                                         width:static_cast<NSUInteger>(width)
                                                        height:static_cast<NSUInteger>(height)
                                                     mipmapped:NO];
  td.usage = MTLTextureUsageShaderRead;
  id<MTLTexture> tex = [impl_->device newTextureWithDescriptor:td iosurface:surf plane:0];
  if (!tex) {
    return Result<Texture>::fail("Metal IOSurface texture failed");
  }
  Texture t;
  t.handle_ = impl_->next++;
  t.width_ = width;
  t.height_ = height;
  if (t.handle_ >= impl_->textures.size()) {
    impl_->textures.resize(t.handle_ + 1);
  }
  impl_->textures[t.handle_] = tex;
  t.native_ = (__bridge void*)tex;
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
    return Result<Shader>::fail(
        msg ? std::string(msg) : std::string("Metal shader compile failed")
    );
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
  pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
  pd.colorAttachments[0].blendingEnabled = YES;
  pd.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
  pd.colorAttachments[0].destinationRGBBlendFactor = dstFactor(desc.blend);
  pd.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
  pd.colorAttachments[0].destinationAlphaBlendFactor = dstFactor(desc.blend);
  pd.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
  pd.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;

  NSError* error = nil;
  id<MTLRenderPipelineState> pso = [impl_->device newRenderPipelineStateWithDescriptor:pd
                                                                                 error:&error];
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

void* Device::nativeDevice() const { return impl_ ? (__bridge void*)impl_->device : nullptr; }

void* Device::nativeLayer() const { return impl_ ? (__bridge void*)impl_->layer : nullptr; }

void* Device::nativeSampler() const { return impl_ ? (__bridge void*)impl_->sampler : nullptr; }

void* Device::colorNative() const {
  return impl_ && impl_->currentDrawable ? (__bridge void*)impl_->currentDrawable.texture : nullptr;
}

}  // namespace glim::gpu
