#include "VulkanImpl.h"

namespace glim::gpu {

using namespace vulkan;

Queue::Queue() = default;
Queue::Queue(void* native) : native_(native) {}

Pass::Pass() : impl_(std::make_unique<Impl>()) {}
Pass::~Pass() {
  if (impl_ && impl_->cmd && !impl_->ended) {
    vkCmdEndRenderPass(impl_->cmd);
    if (impl_->target && !impl_->target->swapchain) {
      imageBarrier(
        impl_->cmd,
        impl_->target->image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
      );
      impl_->target->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    } else if (impl_->target) {
      impl_->target->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    impl_->ended = true;
  }
}
Pass::Pass(Pass&&) noexcept = default;
Pass& Pass::operator=(Pass&&) noexcept = default;

void Pass::setPipeline(const Pipeline& pipeline) {
  impl_->pipeline = reinterpret_cast<VkPipeline>(pipeline.native());
  if (impl_->cmd && impl_->pipeline) {
    vkCmdBindPipeline(impl_->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, impl_->pipeline);
  }
}

void Pass::setBindings(std::uint32_t, const Bindings&) {}

void Pass::setVertexBuffer(std::uint32_t, const Buffer&, std::uint64_t) {}

void Pass::setBytes(std::uint32_t index, const void* data, std::uint64_t size) {
  auto& dst = index == 0 ? impl_->bytes0 : impl_->bytes1;
  dst.resize(static_cast<size_t>(size));
  if (data && size) {
    std::memcpy(dst.data(), data, static_cast<size_t>(size));
  }
}

void Pass::setFragmentBytes(std::uint32_t, const void*, std::uint64_t) {}

void Pass::setFragmentTexture(std::uint32_t index, void* nativeTexture) {
  if (index < static_cast<std::uint32_t>(kMaxFragmentTextures)) {
    impl_->fragmentViews[index] = nativeTexture;
  }
}

void Pass::setFragmentSampler(std::uint32_t index, void* nativeSampler) {
  if (index < static_cast<std::uint32_t>(kMaxFragmentSamplers)) {
    impl_->fragmentSamplers[index] = nativeSampler;
  }
}

void Pass::setViewport(float x, float y, float w, float h, float z0, float z1) {
  // Negative height: Vulkan framebuffer Y-down, paint ortho is Y-up NDC (Metal).
  VkViewport vp{};
  vp.x = x;
  vp.y = y + h;
  vp.width = w;
  vp.height = -h;
  vp.minDepth = z0;
  vp.maxDepth = z1;
  vkCmdSetViewport(impl_->cmd, 0, 1, &vp);
}

void Pass::setScissor(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h) {
  VkRect2D s{};
  s.offset = {static_cast<int32_t>(x), static_cast<int32_t>(y)};
  s.extent = {w, h};
  vkCmdSetScissor(impl_->cmd, 0, 1, &s);
}

void Pass::draw(
  std::uint32_t vertexCount,
  std::uint32_t instanceCount,
  std::uint32_t firstVertex,
  std::uint32_t firstInstance
) {
  Device::Impl* d = impl_->device;
  Frame& frame = d->frames[d->frameIndex];
  auto upload = [&](const std::vector<uint8_t>& src, VkDeviceSize alignment) -> VkDeviceSize {
    if (src.empty()) {
      return 0;
    }
    frame.ringOffset = alignUp64(frame.ringOffset, alignment);
    const VkDeviceSize regionStart = static_cast<VkDeviceSize>(d->frameIndex) * kRingBytes;
    const VkDeviceSize regionEnd = regionStart + kRingBytes;
    if (frame.ringOffset < regionStart || frame.ringOffset + src.size() > regionEnd) {
      frame.ringOffset = regionStart;
    }
    const VkDeviceSize off = frame.ringOffset;
    std::memcpy(static_cast<uint8_t*>(d->ring.mapped) + off, src.data(), src.size());
    frame.ringOffset += src.size();
    return off;
  };
  const VkDeviceSize off0 = upload(impl_->bytes0, d->ringAlign);
  const VkDeviceSize off1 = upload(impl_->bytes1, d->ringAlign);

  VkDescriptorSetAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  ai.descriptorPool = frame.descriptors;
  ai.descriptorSetCount = 1;
  ai.pSetLayouts = &d->setLayout;
  VkDescriptorSet set = VK_NULL_HANDLE;
  if (vkAllocateDescriptorSets(d->device, &ai, &set) != VK_SUCCESS) {
    return;
  }

  VkDescriptorBufferInfo ssbo{};
  ssbo.buffer = d->ring.buffer;
  ssbo.offset = impl_->bytes0.empty() ? 0 : off0;
  ssbo.range = impl_->bytes0.empty() ? 16 : impl_->bytes0.size();
  VkDescriptorBufferInfo uniforms{};
  uniforms.buffer = d->ring.buffer;
  uniforms.offset = impl_->bytes1.empty() ? 0 : off1;
  uniforms.range = impl_->bytes1.empty() ? 64 : impl_->bytes1.size();
  VkDescriptorImageInfo images[kMaxFragmentTextures]{};
  for (int i = 0; i < kMaxFragmentTextures; ++i) {
    images[i].imageView = impl_->fragmentViews[i]
                            ? reinterpret_cast<VkImageView>(impl_->fragmentViews[i])
                            : d->dummyImage.view;
    images[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }
  VkDescriptorImageInfo samplers[kMaxFragmentSamplers]{};
  for (int i = 0; i < kMaxFragmentSamplers; ++i) {
    samplers[i].sampler = impl_->fragmentSamplers[i]
                            ? reinterpret_cast<VkSampler>(impl_->fragmentSamplers[i])
                            : d->sampler;
  }

  VkWriteDescriptorSet writes[kBindingCount]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = set;
  writes[0].dstBinding = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[0].pBufferInfo = &ssbo;
  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = set;
  writes[1].dstBinding = 1;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[1].pBufferInfo = &uniforms;
  for (int i = 0; i < kMaxFragmentTextures; ++i) {
    writes[2 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2 + i].dstSet = set;
    writes[2 + i].dstBinding = static_cast<uint32_t>(2 + i);
    writes[2 + i].descriptorCount = 1;
    writes[2 + i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[2 + i].pImageInfo = &images[i];
  }
  for (int i = 0; i < kMaxFragmentSamplers; ++i) {
    VkWriteDescriptorSet& w = writes[2 + kMaxFragmentTextures + i];
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = set;
    w.dstBinding = static_cast<uint32_t>(kSamplerBinding + i);
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    w.pImageInfo = &samplers[i];
  }
  vkUpdateDescriptorSets(d->device, kBindingCount, writes, 0, nullptr);
  vkCmdBindDescriptorSets(
    impl_->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, d->pipelineLayout, 0, 1, &set, 0, nullptr
  );
  vkCmdDraw(impl_->cmd, vertexCount, instanceCount, firstVertex, firstInstance);
}

void Pass::drawIndexed(std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t) {}

void Pass::end() {
  if (!impl_ || impl_->ended || !impl_->cmd) {
    return;
  }
  vkCmdEndRenderPass(impl_->cmd);
  if (impl_->target && !impl_->target->swapchain) {
    imageBarrier(
      impl_->cmd,
      impl_->target->image,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    );
    impl_->target->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  } else if (impl_->target) {
    impl_->target->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }
  impl_->ended = true;
}

CommandEncoder::CommandEncoder() : impl_(std::make_unique<Impl>()) {}
CommandEncoder::~CommandEncoder() = default;
CommandEncoder::CommandEncoder(CommandEncoder&&) noexcept = default;
CommandEncoder& CommandEncoder::operator=(CommandEncoder&&) noexcept = default;

Pass CommandEncoder::beginPass(const PassDesc& desc) {
  Pass pass;
  Device::Impl* d = impl_->device;
  GpuImage* img = d->imageFromView(desc.nativeColor);
  pass.impl_->device = d;
  pass.impl_->cmd = impl_->cmd;
  pass.impl_->target = img;
  for (int i = 0; i < kMaxFragmentTextures; ++i) {
    pass.impl_->fragmentViews[i] = nullptr;
  }
  if (!img || !impl_->cmd) {
    pass.impl_->ended = true;
    return pass;
  }

  VkRenderPass rp = d->rpClear;
  switch (desc.load) {
    case LoadOp::Load:
      rp = d->rpLoad;
      break;
    case LoadOp::DontCare:
      rp = d->rpDontCare;
      break;
    case LoadOp::Clear:
    default:
      rp = d->rpClear;
      break;
  }

  if (img->layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    VkAccessFlags src = 0;
    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (img->layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      src = VK_ACCESS_SHADER_READ_BIT;
      srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (img->layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
      src = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (img->layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
      src = 0;
      srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    imageBarrier(
      impl_->cmd,
      img->image,
      img->layout,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      src,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      srcStage,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    );
    img->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  VkClearValue clear{};
  clear.color = {{desc.clear.rgba[0], desc.clear.rgba[1], desc.clear.rgba[2], desc.clear.rgba[3]}};
  VkRenderPassBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  bi.renderPass = rp;
  bi.framebuffer = img->framebuffer;
  bi.renderArea.extent = {static_cast<uint32_t>(img->width), static_cast<uint32_t>(img->height)};
  bi.clearValueCount = 1;
  bi.pClearValues = &clear;
  vkCmdBeginRenderPass(impl_->cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);
  img->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  const int vw = desc.viewportW > 0 ? desc.viewportW : img->width;
  const int vh = desc.viewportH > 0 ? desc.viewportH : img->height;
  pass.setViewport(0, 0, static_cast<float>(vw), static_cast<float>(vh), 0, 1);
  pass.setScissor(0, 0, static_cast<uint32_t>(vw), static_cast<uint32_t>(vh));
  return pass;
}

bool CommandEncoder::copyColorTo(const FrameTarget& dst) {
  Device::Impl* d = impl_->device;
  if (!d || !impl_->cmd || !dst.native()) {
    return false;
  }
  GpuImage* src = d->imageFromView(nullptr);
  GpuImage* dest = d->imageFromView(dst.native());
  if (!src || !src->image || !dest || !dest->image) {
    return false;
  }
  auto barrier = [&](
                   GpuImage* img,
                   VkImageLayout neu,
                   VkAccessFlags srcAccess,
                   VkAccessFlags dstAccess,
                   VkPipelineStageFlags srcStage,
                   VkPipelineStageFlags dstStage
                 ) {
    imageBarrier(
      impl_->cmd, img->image, img->layout, neu, srcAccess, dstAccess, srcStage, dstStage
    );
    img->layout = neu;
  };
  if (src->layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    VkAccessFlags sa = 0;
    VkPipelineStageFlags ss = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (src->layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
      sa = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      ss = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (src->layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      sa = VK_ACCESS_SHADER_READ_BIT;
      ss = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    barrier(
      src,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      sa,
      VK_ACCESS_TRANSFER_READ_BIT,
      ss,
      VK_PIPELINE_STAGE_TRANSFER_BIT
    );
  }
  if (dest->layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    VkAccessFlags sa = 0;
    VkPipelineStageFlags ss = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (dest->layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      sa = VK_ACCESS_SHADER_READ_BIT;
      ss = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (dest->layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
      sa = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      ss = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    barrier(
      dest,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      sa,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      ss,
      VK_PIPELINE_STAGE_TRANSFER_BIT
    );
  }
  VkImageBlit region{};
  region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.srcSubresource.layerCount = 1;
  region.srcOffsets[1] = {src->width, src->height, 1};
  region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.dstSubresource.layerCount = 1;
  region.dstOffsets[1] = {dest->width, dest->height, 1};
  vkCmdBlitImage(
    impl_->cmd,
    src->image,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    dest->image,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1,
    &region,
    VK_FILTER_LINEAR
  );
  barrier(
    dest,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_ACCESS_TRANSFER_WRITE_BIT,
    VK_ACCESS_SHADER_READ_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
  );
  barrier(
    src,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_ACCESS_TRANSFER_READ_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  );
  return true;
}

void CommandEncoder::generateMips(const FrameTarget&) {}

void CommandEncoder::present(const Drawable&) { impl_->presentQueued = true; }

void CommandEncoder::submit(Queue&) {
  Device::Impl* d = impl_->device;
  if (!d || !impl_->recording) {
    return;
  }
  Frame& frame = d->frames[impl_->frame];
  if (impl_->presentQueued && impl_->imageIndex < d->swapImages.size()) {
    GpuImage& img = d->swapImages[impl_->imageIndex];
    if (img.layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
      imageBarrier(
        impl_->cmd,
        img.image,
        img.layout,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
      );
      img.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
  }
  d->endTimestamp(impl_->cmd, impl_->frame);
  vkEndCommandBuffer(impl_->cmd);
  impl_->recording = false;

  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  const bool noSync = d->isHeadless;
  si.waitSemaphoreCount = noSync ? 0 : 1;
  si.pWaitSemaphores = &frame.imageAvailable;
  si.pWaitDstStageMask = &waitStage;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &impl_->cmd;
  si.signalSemaphoreCount = noSync ? 0 : 1;
  si.pSignalSemaphores = &frame.renderFinished;
  vkQueueSubmit(d->queue, 1, &si, frame.inFlight);

  if (impl_->presentQueued) {
    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &frame.renderFinished;
    pi.swapchainCount = 1;
    pi.pSwapchains = &d->swapchain;
    pi.pImageIndices = &impl_->imageIndex;
    const VkResult r = vkQueuePresentKHR(d->queue, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
      d->recreateSwapchain();
    }
    if (d->onPresented) {
      std::exchange(d->onPresented, nullptr)();
    }
  }
}

}  // namespace glim::gpu
