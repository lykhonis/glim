#include "WebGpuImpl.h"

namespace glim::gpu {

using namespace webgpu;

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
  dev->surfaceFormat = w.format;
  dev->format = srgbVariant(w.format);
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

Queue& Device::queue() { return impl_->queueWrapper; }

Result<Drawable> Device::nextDrawable() {
  WebBootstrap& w = web();
  if (!impl_ || !w.ready) {
    static bool logged = false;
    if (!logged) {
      logged = true;
      printf(
        "[glim-web] nextDrawable: not ready ready=%d device=%p\n", (int)w.ready, (void*)w.device
      );
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
  impl_->swapView = makeSingleMipView(st.texture, impl_->format);
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

Extent Device::drawableSize() const {
  if (!impl_) {
    return {};
  }
  return {impl_->drawableW, impl_->drawableH};
}

int Device::presentRotationDegrees() const { return 0; }

float Device::lastGpuMs() const { return 0.f; }

void Device::notifyNextPresented(std::function<void()> fn) { impl_->onPresented = std::move(fn); }

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
  td.usage = static_cast<WGPUTextureUsage>(
    WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding
  );
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

bool Device::headless() const { return false; }

bool Device::readPixels(const FrameTarget&, void*, std::uint64_t) { return false; }

Result<Buffer> Device::createBuffer(const BufferDesc& desc) {
  WGPUBufferUsage usage =
    static_cast<WGPUBufferUsage>(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);
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
  wgpuQueueWriteBuffer(
    impl_->queue,
    static_cast<WGPUBuffer>(buffer.native()),
    0,
    data,
    static_cast<size_t>(std::min<std::uint64_t>(size, buffer.size()))
  );
}

Result<Texture> Device::createTexture(const TextureDesc& desc) {
  const int w = std::max(desc.width, 1);
  const int h = std::max(desc.height, 1);
  WGPUTextureDescriptor td{};
  td.usage =
    static_cast<WGPUTextureUsage>(WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);
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
    wgpuQueueWriteTexture(impl_->queue, &dst, rgba8, static_cast<size_t>(need), &layout, &extent);
    return;
  }
  std::vector<std::uint8_t> padded(static_cast<std::size_t>(dstRow) * hh);
  const auto* src = static_cast<const std::uint8_t*>(rgba8);
  for (std::uint32_t y = 0; y < hh; ++y) {
    std::memcpy(
      padded.data() + static_cast<std::size_t>(y) * dstRow,
      src + static_cast<std::size_t>(y) * srcRow,
      srcRow
    );
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

std::vector<Result<Shader>> Device::createShaders(std::span<const ShaderSource> sources) {
  return createShadersInOrder(*this, sources);
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
  comp.dstFactor =
    desc.blend == Blend::Plus ? WGPUBlendFactor_One : WGPUBlendFactor_OneMinusSrcAlpha;
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

void* Device::nativeDevice() const { return impl_ ? static_cast<void*>(impl_->device) : nullptr; }

void* Device::nativeLayer() const { return impl_ ? static_cast<void*>(impl_->surface) : nullptr; }

void* Device::nativeSampler() const { return impl_ ? static_cast<void*>(impl_->sampler) : nullptr; }

void* Device::colorNative() const { return impl_ ? static_cast<void*>(impl_->swapView) : nullptr; }

}  // namespace glim::gpu
