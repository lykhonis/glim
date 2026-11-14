#include "WebGpuImpl.h"

namespace glim::gpu {

using namespace webgpu;

Queue::Queue() = default;
Queue::Queue(void* native) : native_(native) {}

Pass::Pass() : impl_(std::make_unique<Impl>()) {}
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
  if (impl_ && index < static_cast<std::uint32_t>(kMaxFragmentTextures)) {
    impl_->tex[index] = static_cast<WGPUTextureView>(nativeTexture);
  }
}

void Pass::setFragmentSampler(std::uint32_t index, void* nativeSampler) {
  if (impl_ && index < static_cast<std::uint32_t>(kMaxFragmentSamplers)) {
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

void Pass::draw(
  std::uint32_t vertexCount,
  std::uint32_t instanceCount,
  std::uint32_t firstVertex,
  std::uint32_t firstInstance
) {
  if (!impl_ || !impl_->pass || !impl_->pipeline || !impl_->dev) {
    return;
  }
  Device::Impl* dev = impl_->dev;
  WGPUBuffer b0 = impl_->b0 != nullptr ? impl_->b0 : dev->zeroBuf;
  WGPUBuffer b1 = impl_->b1 != nullptr ? impl_->b1 : dev->zeroBuf;
  WGPUBindGroupEntry entries[kBindingCount]{};
  entries[0].binding = 0;
  entries[0].buffer = b0;
  entries[0].offset = impl_->b0 != nullptr ? impl_->b0off : 0;
  entries[0].size = impl_->b0 != nullptr ? impl_->b0size : 64;
  entries[1].binding = 1;
  entries[1].buffer = b1;
  entries[1].offset = impl_->b1 != nullptr ? impl_->b1off : 0;
  entries[1].size = impl_->b1 != nullptr ? impl_->b1size : 64;
  for (int i = 0; i < kMaxFragmentTextures; ++i) {
    entries[kTextureBinding + i].binding = static_cast<std::uint32_t>(kTextureBinding + i);
    entries[kTextureBinding + i].textureView =
      impl_->tex[i] != nullptr ? impl_->tex[i] : dev->dummyView;
  }
  for (int i = 0; i < kMaxFragmentSamplers; ++i) {
    entries[kSamplerBinding + i].binding = static_cast<std::uint32_t>(kSamplerBinding + i);
    entries[kSamplerBinding + i].sampler =
      impl_->samp[i] != nullptr ? impl_->samp[i] : dev->sampler;
  }
  WGPUBindGroupDescriptor bgd{};
  bgd.layout = dev->bindLayout;
  bgd.entryCount = kBindingCount;
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

CommandEncoder::CommandEncoder() : impl_(std::make_unique<Impl>()) {}
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
  color.clearValue = {
    desc.clear.rgba[0], desc.clear.rgba[1], desc.clear.rgba[2], desc.clear.rgba[3]
  };
  WGPURenderPassDescriptor rpd{};
  rpd.colorAttachmentCount = 1;
  rpd.colorAttachments = &color;
  pass.impl_->dev = dev;
  pass.impl_->encoder = impl_->encoder;
  pass.impl_->pass = wgpuCommandEncoderBeginRenderPass(impl_->encoder, &rpd);
  if (desc.viewportW > 0 && desc.viewportH > 0) {
    pass.setViewport(
      0, 0, static_cast<float>(desc.viewportW), static_cast<float>(desc.viewportH), 0, 1
    );
  }
  return pass;
}

bool CommandEncoder::copyColorTo(const FrameTarget&) { return false; }

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
    if (dev->onPresented) {
      std::exchange(dev->onPresented, nullptr)();
    }
  }
  dev->ringIndex = (dev->ringIndex + 1) % kRingFrames;
  dev->ringOffset = 0;
}

}  // namespace glim::gpu
