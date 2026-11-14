#include "VulkanImpl.h"

namespace glim::gpu {

using namespace vulkan;

void Device::Impl::destroyImage(GpuImage& img) {
  if (!device) {
    return;
  }
  if (img.borrowed) {
    img = {};
    return;
  }
  if (img.framebuffer) {
    vkDestroyFramebuffer(device, img.framebuffer, nullptr);
  }
  if (img.view) {
    vkDestroyImageView(device, img.view, nullptr);
  }
  if (img.image && !img.swapchain) {
    vkDestroyImage(device, img.image, nullptr);
  }
  if (img.memory) {
    vkFreeMemory(device, img.memory, nullptr);
  }
  img = {};
}

void Device::Impl::destroyBuffer(GpuBuffer& buf) {
  if (!device) {
    return;
  }
  if (buf.buffer) {
    vkDestroyBuffer(device, buf.buffer, nullptr);
  }
  if (buf.memory) {
    vkFreeMemory(device, buf.memory, nullptr);
  }
  buf = {};
}

void Device::Impl::destroy() {
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  androidUnregisterDevice(this);
#endif
  if (device) {
    vkDeviceWaitIdle(device);
  }
  destroySwapchain();
  for (GpuImage& t : targets) {
    destroyImage(t);
  }
  targets.clear();
  for (GpuImage& t : sampled) {
    destroyImage(t);
  }
  sampled.clear();
  for (GpuBuffer& b : buffers) {
    destroyBuffer(b);
  }
  buffers.clear();
  for (PipelineRec& p : pipelines) {
    if (device && p.pipeline) {
      vkDestroyPipeline(device, p.pipeline, nullptr);
    }
  }
  pipelines.clear();
  for (ShaderRec& s : shaders) {
    if (device && s.module) {
      vkDestroyShaderModule(device, s.module, nullptr);
    }
  }
  shaders.clear();
  destroyImage(dummyImage);
  destroyBuffer(ring);
  destroyBuffer(readStaging);
  if (device) {
    if (sampler) {
      vkDestroySampler(device, sampler, nullptr);
    }
    if (pipelineLayout) {
      vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
    if (setLayout) {
      vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
    }
    if (rpClear) {
      vkDestroyRenderPass(device, rpClear, nullptr);
    }
    if (rpLoad) {
      vkDestroyRenderPass(device, rpLoad, nullptr);
    }
    if (rpDontCare) {
      vkDestroyRenderPass(device, rpDontCare, nullptr);
    }
    for (Frame& f : frames) {
      if (f.inFlight) {
        vkDestroyFence(device, f.inFlight, nullptr);
      }
      if (f.imageAvailable) {
        vkDestroySemaphore(device, f.imageAvailable, nullptr);
      }
      if (f.renderFinished) {
        vkDestroySemaphore(device, f.renderFinished, nullptr);
      }
      if (f.descriptors) {
        vkDestroyDescriptorPool(device, f.descriptors, nullptr);
      }
    }
    if (timestamps) {
      vkDestroyQueryPool(device, timestamps, nullptr);
    }
    if (cmdPool) {
      vkDestroyCommandPool(device, cmdPool, nullptr);
    }
    if (uploadPool) {
      vkDestroyCommandPool(device, uploadPool, nullptr);
    }
    vkDestroyDevice(device, nullptr);
  }
  if (instance && surface) {
    vkDestroySurfaceKHR(instance, surface, nullptr);
  }
  if (instance) {
    vkDestroyInstance(instance, nullptr);
  }
  instance = VK_NULL_HANDLE;
  surface = VK_NULL_HANDLE;
  device = VK_NULL_HANDLE;
  physical = VK_NULL_HANDLE;
  queue = VK_NULL_HANDLE;
  cmdPool = VK_NULL_HANDLE;
  uploadPool = VK_NULL_HANDLE;
  sampler = VK_NULL_HANDLE;
  pipelineLayout = VK_NULL_HANDLE;
  setLayout = VK_NULL_HANDLE;
  rpClear = rpLoad = rpDontCare = VK_NULL_HANDLE;
  for (Frame& f : frames) {
    f = {};
  }
}

VkResult Device::Impl::makeBuffer(
  VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memFlags, GpuBuffer& out
) {
  destroyBuffer(out);
  VkBufferCreateInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bi.size = std::max<VkDeviceSize>(size, 16);
  bi.usage = usage;
  bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VkResult r = vkCreateBuffer(device, &bi, nullptr, &out.buffer);
  if (r != VK_SUCCESS) {
    return r;
  }
  VkMemoryRequirements req{};
  vkGetBufferMemoryRequirements(device, out.buffer, &req);
  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = req.size;
  ai.memoryTypeIndex = findMemoryType(physical, req.memoryTypeBits, memFlags);
  if (ai.memoryTypeIndex == UINT32_MAX) {
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
  }
  r = vkAllocateMemory(device, &ai, nullptr, &out.memory);
  if (r != VK_SUCCESS) {
    return r;
  }
  vkBindBufferMemory(device, out.buffer, out.memory, 0);
  out.size = bi.size;
  if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    r = vkMapMemory(device, out.memory, 0, bi.size, 0, &out.mapped);
  }
  return r;
}

VkResult Device::Impl::makeImage(
  int w, int h, VkImageUsageFlags usage, GpuImage& out, bool sampled, VkFormat imgFormat
) {
  destroyImage(out);
  out.width = std::max(w, 1);
  out.height = std::max(h, 1);
  const VkFormat useFormat = imgFormat == VK_FORMAT_UNDEFINED ? format : imgFormat;
  VkImageCreateInfo ii{};
  ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.format = useFormat;
  ii.extent = {static_cast<uint32_t>(out.width), static_cast<uint32_t>(out.height), 1};
  ii.mipLevels = 1;
  ii.arrayLayers = 1;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  ii.tiling = VK_IMAGE_TILING_OPTIMAL;
  ii.usage = usage;
  ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkResult r = vkCreateImage(device, &ii, nullptr, &out.image);
  if (r != VK_SUCCESS) {
    return r;
  }
  VkMemoryRequirements req{};
  vkGetImageMemoryRequirements(device, out.image, &req);
  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = req.size;
  ai.memoryTypeIndex =
    findMemoryType(physical, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (ai.memoryTypeIndex == UINT32_MAX) {
    ai.memoryTypeIndex = findMemoryType(physical, req.memoryTypeBits, 0);
  }
  if (ai.memoryTypeIndex == UINT32_MAX) {
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
  }
  r = vkAllocateMemory(device, &ai, nullptr, &out.memory);
  if (r != VK_SUCCESS) {
    return r;
  }
  vkBindImageMemory(device, out.image, out.memory, 0);

  VkImageViewCreateInfo vi{};
  vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  vi.image = out.image;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = useFormat;
  vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vi.subresourceRange.levelCount = 1;
  vi.subresourceRange.layerCount = 1;
  r = vkCreateImageView(device, &vi, nullptr, &out.view);
  if (r != VK_SUCCESS) {
    return r;
  }
  if (rpLoad && (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) {
    VkFramebufferCreateInfo fb{};
    fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass = rpLoad;
    fb.attachmentCount = 1;
    fb.pAttachments = &out.view;
    fb.width = static_cast<uint32_t>(out.width);
    fb.height = static_cast<uint32_t>(out.height);
    fb.layers = 1;
    r = vkCreateFramebuffer(device, &fb, nullptr, &out.framebuffer);
  }
  (void)sampled;
  out.layout = VK_IMAGE_LAYOUT_UNDEFINED;
  return r;
}

VkRenderPass Device::Impl::makeRenderPass(
  VkAttachmentLoadOp load, VkAttachmentStoreOp store, VkImageLayout initial
) {
  VkAttachmentDescription color{};
  color.format = format;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = load;
  color.storeOp = store;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = initial;
  color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference ref{};
  ref.attachment = 0;
  ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription sub{};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.colorAttachmentCount = 1;
  sub.pColorAttachments = &ref;

  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask =
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  ci.attachmentCount = 1;
  ci.pAttachments = &color;
  ci.subpassCount = 1;
  ci.pSubpasses = &sub;
  ci.dependencyCount = 1;
  ci.pDependencies = &dep;
  VkRenderPass rp = VK_NULL_HANDLE;
  vkCreateRenderPass(device, &ci, nullptr, &rp);
  return rp;
}

GpuImage* Device::Impl::imageFromView(void* view) {
  if (!view) {
    if (imageIndex < swapImages.size()) {
      return &swapImages[imageIndex];
    }
    return nullptr;
  }
  const VkImageView v = reinterpret_cast<VkImageView>(view);
  for (GpuImage& img : swapImages) {
    if (img.view == v) {
      return &img;
    }
  }
  for (GpuImage& img : targets) {
    if (img.view == v) {
      return &img;
    }
  }
  return nullptr;
}

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
std::vector<Device::Impl*>& androidDevices() {
  static std::vector<Device::Impl*> devices;
  return devices;
}

void androidUnregisterDevice(void* impl) {
  auto& list = androidDevices();
  list.erase(std::remove(list.begin(), list.end(), static_cast<Device::Impl*>(impl)), list.end());
}
#endif

void Device::Impl::readGpuMs(uint32_t slot) {
  if (!timestamps || !timed[slot]) return;
  uint64_t ticks[2]{};
  const VkResult r = vkGetQueryPoolResults(
    device, timestamps, 2 * slot, 2, sizeof(ticks), ticks, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT
  );
  timed[slot] = false;
  if (r == VK_SUCCESS && ticks[1] >= ticks[0]) {
    gpuMs = static_cast<float>(
      static_cast<double>(ticks[1] - ticks[0]) * props.limits.timestampPeriod / 1.0e6
    );
  }
}

void Device::Impl::beginTimestamp(VkCommandBuffer cmd, uint32_t slot) {
  if (!timestamps) return;
  vkCmdResetQueryPool(cmd, timestamps, 2 * slot, 2);
  vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestamps, 2 * slot);
}

void Device::Impl::endTimestamp(VkCommandBuffer cmd, uint32_t slot) {
  if (!timestamps) return;
  vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamps, 2 * slot + 1);
  timed[slot] = true;
}

}  // namespace glim::gpu
