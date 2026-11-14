#include "VulkanImpl.h"

namespace glim::gpu {

using namespace vulkan;

Device::Device() = default;
Device::~Device() = default;
Device::Device(Device&&) noexcept = default;
Device& Device::operator=(Device&&) noexcept = default;

Queue& Device::queue() { return impl_->queueWrapper; }

void Device::setDrawableSize(int width, int height) {
  if (!impl_ || width <= 0 || height <= 0) {
    return;
  }
  impl_->wantedWidth = static_cast<uint32_t>(width);
  impl_->wantedHeight = static_cast<uint32_t>(height);
}

Extent Device::drawableSize() const {
  if (!impl_ || impl_->isHeadless || !impl_->haveSwapchain) {
    return {};
  }
  return {static_cast<int>(impl_->extent.width), static_cast<int>(impl_->extent.height)};
}

float Device::lastGpuMs() const { return 0.f; }

void Device::notifyNextPresented(std::function<void()> fn) { impl_->onPresented = std::move(fn); }

int Device::presentRotationDegrees() const {
  if (!impl_) {
    return 0;
  }
  switch (impl_->preTransform) {
    case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
      return 270;
    case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
      return 180;
    case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
      return 90;
    default:
      return 0;
  }
}

Result<Drawable> Device::nextDrawable() {
  if (!impl_ || impl_->isHeadless) {
    return Result<Drawable>::fail("headless device has no drawable");
  }
  for (GpuImage& t : impl_->targets) {
    t.inUse = false;
  }
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  if (!impl_->surface) {
    return Result<Drawable>::fail("Android window is gone");
  }
  if (impl_->androidWindow) {
    const int aw = ANativeWindow_getWidth(impl_->androidWindow);
    const int ah = ANativeWindow_getHeight(impl_->androidWindow);
    if (aw > 0 && ah > 0) {
      impl_->wantedWidth = static_cast<uint32_t>(aw);
      impl_->wantedHeight = static_cast<uint32_t>(ah);
    }
  }
#endif
  {
    VkSurfaceCapabilitiesKHR caps{};
    if (impl_->surface &&
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(impl_->physical, impl_->surface, &caps) ==
          VK_SUCCESS) {
      const VkExtent2D want = impl_->chooseSwapchainExtent(caps);
      if (!impl_->haveSwapchain || want.width != impl_->extent.width ||
          want.height != impl_->extent.height || caps.currentTransform != impl_->preTransform) {
        const VkResult rebuilt = impl_->recreateSwapchain();
        if (rebuilt != VK_SUCCESS && rebuilt != VK_NOT_READY) {
          return Result<Drawable>::fail("swapchain recreate failed");
        }
      }
    } else if (!impl_->haveSwapchain) {
      const VkResult r = impl_->recreateSwapchain();
      if (r != VK_SUCCESS) {
        return Result<Drawable>::fail("swapchain create failed");
      }
    }
  }
  impl_->frameIndex = (impl_->frameIndex + 1) % kFrames;
  Frame& frame = impl_->frames[impl_->frameIndex];
  vkWaitForFences(impl_->device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);
  vkResetFences(impl_->device, 1, &frame.inFlight);
  vkResetDescriptorPool(impl_->device, frame.descriptors, 0);
  frame.ringOffset = static_cast<VkDeviceSize>(impl_->frameIndex) * kRingBytes;

  uint32_t index = 0;
  VkResult r = vkAcquireNextImageKHR(
    impl_->device, impl_->swapchain, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE, &index
  );
  if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
    if (impl_->recreateSwapchain() != VK_SUCCESS) {
      return Result<Drawable>::fail("swapchain recreate failed");
    }
    r = vkAcquireNextImageKHR(
      impl_->device, impl_->swapchain, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE, &index
    );
  }
  if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
    return Result<Drawable>::fail("vkAcquireNextImageKHR failed");
  }
  impl_->imageIndex = index;
  Drawable d;
  d.handle_ = 1;
  d.native_ = impl_->swapImages[index].view;
  d.width_ = static_cast<int>(impl_->extent.width);
  d.height_ = static_cast<int>(impl_->extent.height);
  return Result<Drawable>::ok(d);
}

CommandEncoder Device::encoder() {
  CommandEncoder enc;
  if (impl_->isHeadless) {
    impl_->frameIndex = (impl_->frameIndex + 1) % kFrames;
    Frame& hframe = impl_->frames[impl_->frameIndex];
    vkWaitForFences(impl_->device, 1, &hframe.inFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(impl_->device, 1, &hframe.inFlight);
    vkResetDescriptorPool(impl_->device, hframe.descriptors, 0);
    hframe.ringOffset = static_cast<VkDeviceSize>(impl_->frameIndex) * kRingBytes;
  }
  Frame& frame = impl_->frames[impl_->frameIndex];
  vkResetCommandBuffer(frame.cmd, 0);
  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(frame.cmd, &bi);
  enc.impl_->device = impl_.get();
  enc.impl_->frame = impl_->frameIndex;
  enc.impl_->imageIndex = impl_->imageIndex;
  enc.impl_->cmd = frame.cmd;
  enc.impl_->recording = true;
  return enc;
}

void* Device::nativeDevice() const { return impl_ ? impl_->device : nullptr; }

void* Device::nativeLayer() const {
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  return impl_ ? impl_->androidWindow : nullptr;
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
  return impl_ ? impl_->wlSurface : nullptr;
#else
  return nullptr;
#endif
}

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
void setAndroidNativeWindow(void* native) {
  auto* window = static_cast<ANativeWindow*>(native);
  for (Device::Impl* d : androidDevices()) {
    d->bindAndroidWindow(window);
  }
}
#endif

void* Device::nativeSampler() const { return impl_ ? impl_->sampler : nullptr; }

void* Device::colorNative() const {
  if (!impl_ || impl_->imageIndex >= impl_->swapImages.size()) {
    return nullptr;
  }
  return impl_->swapImages[impl_->imageIndex].view;
}

bool Device::headless() const { return impl_ && impl_->isHeadless; }

bool Device::readPixels(const FrameTarget& target, void* rgba8, std::uint64_t bytes) {
  if (!impl_ || !rgba8 || target.width() <= 0 || target.height() <= 0) {
    return false;
  }
  const std::uint64_t need =
    static_cast<std::uint64_t>(target.width()) * static_cast<std::uint64_t>(target.height()) * 4;
  if (bytes < need) {
    return false;
  }
  Impl& d = *impl_;
  GpuImage* img = d.imageFromView(target.native());
  if (!img || !img->image) {
    return false;
  }
  if (img->width != target.width() || img->height != target.height()) {
    return false;
  }
  const int w = img->width;
  const int h = img->height;
  const VkDeviceSize rowBytes = static_cast<VkDeviceSize>(w) * 4;
  const VkDeviceSize total = rowBytes * static_cast<VkDeviceSize>(h);
  if (!d.readStaging.buffer || d.readStaging.size < total) {
    if (d.makeBuffer(
          total,
          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
          d.readStaging
        ) != VK_SUCCESS ||
        !d.readStaging.mapped) {
      return false;
    }
  }
  if (vkQueueWaitIdle(d.queue) != VK_SUCCESS) {
    return false;
  }
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkCommandBufferAllocateInfo cba{};
  cba.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cba.commandPool = d.uploadPool;
  cba.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cba.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(d.device, &cba, &cmd) != VK_SUCCESS) {
    return false;
  }
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) {
    vkFreeCommandBuffers(d.device, d.uploadPool, 1, &cmd);
    return false;
  }
  VkAccessFlags srcAccess = 0;
  VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  if (img->layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else if (img->layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    srcAccess = VK_ACCESS_SHADER_READ_BIT;
    srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  imageBarrier(
    cmd,
    img->image,
    img->layout,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    srcAccess,
    VK_ACCESS_TRANSFER_READ_BIT,
    srcStage,
    VK_PIPELINE_STAGE_TRANSFER_BIT
  );
  VkBufferImageCopy region{};
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.layerCount = 1;
  region.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
  vkCmdCopyImageToBuffer(
    cmd, img->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, d.readStaging.buffer, 1, &region
  );
  imageBarrier(
    cmd,
    img->image,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_ACCESS_TRANSFER_READ_BIT,
    VK_ACCESS_SHADER_READ_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
  );
  if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
    vkFreeCommandBuffers(d.device, d.uploadPool, 1, &cmd);
    return false;
  }
  VkFence fence = VK_NULL_HANDLE;
  VkFenceCreateInfo fi{};
  fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  if (vkCreateFence(d.device, &fi, nullptr, &fence) != VK_SUCCESS) {
    vkFreeCommandBuffers(d.device, d.uploadPool, 1, &cmd);
    return false;
  }
  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cmd;
  if (vkQueueSubmit(d.queue, 1, &si, fence) != VK_SUCCESS) {
    vkDestroyFence(d.device, fence, nullptr);
    vkFreeCommandBuffers(d.device, d.uploadPool, 1, &cmd);
    return false;
  }
  if (vkWaitForFences(d.device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
    vkDestroyFence(d.device, fence, nullptr);
    vkFreeCommandBuffers(d.device, d.uploadPool, 1, &cmd);
    return false;
  }
  vkDestroyFence(d.device, fence, nullptr);
  vkFreeCommandBuffers(d.device, d.uploadPool, 1, &cmd);
  img->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  const std::uint8_t* src = static_cast<const std::uint8_t*>(d.readStaging.mapped);
  std::uint8_t* dst = static_cast<std::uint8_t*>(rgba8);
  const bool bgra = (d.format == VK_FORMAT_B8G8R8A8_UNORM || d.format == VK_FORMAT_B8G8R8A8_SRGB);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const std::uint8_t* p = src + (static_cast<std::size_t>(y) * w + x) * 4;
      std::uint8_t* q = dst + (static_cast<std::size_t>(y) * w + x) * 4;
      q[0] = bgra ? p[2] : p[0];
      q[1] = p[1];
      q[2] = bgra ? p[0] : p[2];
      q[3] = p[3];
    }
  }
  return true;
}

}  // namespace glim::gpu
