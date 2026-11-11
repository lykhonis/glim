#include "VulkanImpl.h"

namespace glim::gpu {

using namespace vulkan;

void Device::Impl::destroySwapchain() {
  if (!device) {
    return;
  }
  for (GpuImage& img : swapImages) {
    img.image = VK_NULL_HANDLE;
    destroyImage(img);
  }
  swapImages.clear();
  if (swapchain) {
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;
  }
  haveSwapchain = false;
}

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
void Device::Impl::bindAndroidWindow(ANativeWindow* window) {
  if (androidWindow == window && ((window == nullptr && !surface) || (window && surface))) {
    return;
  }
  if (device) {
    vkDeviceWaitIdle(device);
  }
  destroySwapchain();
  if (instance && surface) {
    vkDestroySurfaceKHR(instance, surface, nullptr);
    surface = VK_NULL_HANDLE;
  }
  androidWindow = window;
  if (!window || !instance) {
    return;
  }
  VkAndroidSurfaceCreateInfoKHR sci{};
  sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  sci.window = window;
  vkCreateAndroidSurfaceKHR(instance, &sci, nullptr, &surface);
}
#endif

bool Device::Impl::pickFormat() {
  uint32_t count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, formats.data());
  if (formats.empty()) {
    return false;
  }
  for (const auto& f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_SRGB) {
      format = f.format;
      colorSpace = f.colorSpace;
      return true;
    }
  }
  for (const auto& f : formats) {
    if (f.format == VK_FORMAT_R8G8B8A8_SRGB) {
      format = f.format;
      colorSpace = f.colorSpace;
      return true;
    }
  }
  for (const auto& f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_UNORM) {
      format = f.format;
      colorSpace = f.colorSpace;
      return true;
    }
  }
  for (const auto& f : formats) {
    if (f.format == VK_FORMAT_R8G8B8A8_UNORM) {
      format = f.format;
      colorSpace = f.colorSpace;
      return true;
    }
  }
  format = formats[0].format;
  colorSpace = formats[0].colorSpace;
  return format != VK_FORMAT_UNDEFINED;
}

VkExtent2D Device::Impl::chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& caps) const {
  uint32_t w = 1;
  uint32_t h = 1;
  if (caps.currentExtent.width != UINT32_MAX) {
    w = caps.currentExtent.width;
    h = caps.currentExtent.height;
  } else {
    w = wantedWidth != 0 ? wantedWidth : extent.width;
    h = wantedHeight != 0 ? wantedHeight : extent.height;
    w = std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width);
    h = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
  }
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  if (androidWindow) {
    const int aw = ANativeWindow_getWidth(androidWindow);
    const int ah = ANativeWindow_getHeight(androidWindow);
    if (aw > 0 && ah > 0) {
      w = std::clamp(
          static_cast<uint32_t>(aw), caps.minImageExtent.width, caps.maxImageExtent.width
      );
      h = std::clamp(
          static_cast<uint32_t>(ah), caps.minImageExtent.height, caps.maxImageExtent.height
      );
    }
  }
  if (caps.currentTransform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR ||
      caps.currentTransform == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {
    std::swap(w, h);
  }
#endif
  if (w == 0) {
    w = 1;
  }
  if (h == 0) {
    h = 1;
  }
  return {w, h};
}

VkResult Device::Impl::recreateSwapchain() {
  VkSurfaceCapabilitiesKHR caps{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);
  if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
    return VK_NOT_READY;
  }
  preTransform = caps.currentTransform;
  extent = chooseSwapchainExtent(caps);
  uint32_t modeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &modeCount, nullptr);
  std::vector<VkPresentModeKHR> modes(modeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &modeCount, modes.data());
  VkPresentModeKHR present = VK_PRESENT_MODE_FIFO_KHR;
  if (!vsync) {
    for (auto m : modes) {
      if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) {
        present = m;
        break;
      }
    }
  }

  uint32_t images = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && images > caps.maxImageCount) {
    images = caps.maxImageCount;
  }

  VkSwapchainCreateInfoKHR ci{};
  ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  ci.surface = surface;
  ci.minImageCount = images;
  ci.imageFormat = format;
  ci.imageColorSpace = colorSpace;
  ci.imageExtent = extent;
  ci.imageArrayLayers = 1;
  ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
    ci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ci.preTransform = preTransform;
  ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
  } else if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
  } else if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  }
  ci.presentMode = present;
  ci.clipped = VK_TRUE;
  ci.oldSwapchain = swapchain;

  VkSwapchainKHR neu = VK_NULL_HANDLE;
  VkResult r = vkCreateSwapchainKHR(device, &ci, nullptr, &neu);
  if (r != VK_SUCCESS) {
    return r;
  }
  vkDeviceWaitIdle(device);
  VkSwapchainKHR old = swapchain;
  swapchain = neu;
  for (GpuImage& img : swapImages) {
    img.image = VK_NULL_HANDLE;
    destroyImage(img);
  }
  swapImages.clear();
  if (old) {
    vkDestroySwapchainKHR(device, old, nullptr);
  }

  uint32_t n = 0;
  vkGetSwapchainImagesKHR(device, swapchain, &n, nullptr);
  std::vector<VkImage> raw(n);
  vkGetSwapchainImagesKHR(device, swapchain, &n, raw.data());
  swapImages.resize(n);
  for (uint32_t i = 0; i < n; ++i) {
    GpuImage& img = swapImages[i];
    img.image = raw[i];
    img.swapchain = true;
    img.width = static_cast<int>(extent.width);
    img.height = static_cast<int>(extent.height);
    img.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = img.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    r = vkCreateImageView(device, &vi, nullptr, &img.view);
    if (r != VK_SUCCESS) {
      return r;
    }
    VkFramebufferCreateInfo fb{};
    fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass = rpLoad;
    fb.attachmentCount = 1;
    fb.pAttachments = &img.view;
    fb.width = extent.width;
    fb.height = extent.height;
    fb.layers = 1;
    r = vkCreateFramebuffer(device, &fb, nullptr, &img.framebuffer);
    if (r != VK_SUCCESS) {
      return r;
    }
  }
  haveSwapchain = true;
  return VK_SUCCESS;
}

}  // namespace glim::gpu
