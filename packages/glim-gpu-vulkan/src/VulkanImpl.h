#pragma once

#include <glim/gpu/Device.h>
#include <volk.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#include <android/hardware_buffer.h>
#include <android/native_window.h>
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <wayland-client.h>
#endif

namespace glim::gpu {
namespace vulkan {

inline constexpr uint32_t kFrames = 2;

inline constexpr VkDeviceSize kRingBytes = 2 * 1024 * 1024;

inline VkDeviceSize alignUp64(VkDeviceSize v, VkDeviceSize a) {
  return a ? (v + a - 1) & ~(a - 1) : v;
}

inline VkBlendFactor dstFactor(Blend blend) {
  return blend == Blend::Plus ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
}

inline uint32_t findMemoryType(
  VkPhysicalDevice phys, uint32_t filter, VkMemoryPropertyFlags flags
) {
  VkPhysicalDeviceMemoryProperties props{};
  vkGetPhysicalDeviceMemoryProperties(phys, &props);
  for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
    if ((filter & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags) {
      return i;
    }
  }
  return UINT32_MAX;
}

inline bool hasExtension(const std::vector<VkExtensionProperties>& ext, const char* name) {
  for (const auto& e : ext) {
    if (std::strcmp(e.extensionName, name) == 0) {
      return true;
    }
  }
  return false;
}

inline void imageBarrier(
  VkCommandBuffer cmd,
  VkImage image,
  VkImageLayout oldLayout,
  VkImageLayout newLayout,
  VkAccessFlags srcAccess,
  VkAccessFlags dstAccess,
  VkPipelineStageFlags srcStage,
  VkPipelineStageFlags dstStage
) {
  VkImageMemoryBarrier b{};
  b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  b.srcAccessMask = srcAccess;
  b.dstAccessMask = dstAccess;
  b.oldLayout = oldLayout;
  b.newLayout = newLayout;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image = image;
  b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  b.subresourceRange.levelCount = 1;
  b.subresourceRange.layerCount = 1;
  vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
}

struct GpuImage {
  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
  int width = 0;
  int height = 0;
  bool swapchain = false;
  bool inUse = false;
  bool borrowed = false;
};

struct GpuBuffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
  void* mapped = nullptr;
};

struct ShaderRec {
  VkShaderModule module = VK_NULL_HANDLE;
};

struct PipelineRec {
  VkPipeline pipeline = VK_NULL_HANDLE;
};

struct Frame {
  VkFence inFlight = VK_NULL_HANDLE;
  VkSemaphore imageAvailable = VK_NULL_HANDLE;
  VkSemaphore renderFinished = VK_NULL_HANDLE;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkDescriptorPool descriptors = VK_NULL_HANDLE;
  VkDeviceSize ringOffset = 0;
};

}  // namespace vulkan

struct Device::Impl {
  std::function<void()> onPresented;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  ANativeWindow* androidWindow = nullptr;
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
  wl_display* display = nullptr;
  wl_surface* wlSurface = nullptr;
#endif
  VkInstance instance = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  bool isHeadless = false;
  vulkan::GpuBuffer readStaging{};
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  uint32_t queueFamily = 0;
  Queue queueWrapper;
  bool vsync = true;
  bool haveAhb = false;

  // Linear working space end to end: surfaces are sRGB-encoded,
  // shaders output linear (instance colors arrive linearized,
  // sampled bytes linearize in-shader), hardware converts on write
  // and blends against a decoded destination. Offscreen targets
  // share the format, so readPixels hands back sRGB bytes directly.
  VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;
  VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  VkExtent2D extent{1, 1};
  uint32_t wantedWidth = 0;
  uint32_t wantedHeight = 0;
  VkSurfaceTransformFlagBitsKHR preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  std::vector<vulkan::GpuImage> swapImages;
  uint32_t imageIndex = 0;
  uint32_t frameIndex = 0;
  bool haveSwapchain = false;

  VkRenderPass rpClear = VK_NULL_HANDLE;
  VkRenderPass rpLoad = VK_NULL_HANDLE;
  VkRenderPass rpDontCare = VK_NULL_HANDLE;
  VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  vulkan::GpuImage dummyImage{};
  vulkan::GpuBuffer ring{};
  VkDeviceSize ringAlign = 256;
  VkCommandPool cmdPool = VK_NULL_HANDLE;
  VkCommandPool uploadPool = VK_NULL_HANDLE;
  vulkan::Frame frames[vulkan::kFrames]{};

  std::vector<vulkan::ShaderRec> shaders;
  std::vector<vulkan::PipelineRec> pipelines;
  std::vector<vulkan::GpuBuffer> buffers;
  std::vector<vulkan::GpuImage> targets;
  std::vector<vulkan::GpuImage> sampled;
  Handle next = 1;

  VkPhysicalDeviceProperties props{};

  ~Impl() { destroy(); }

  void destroyImage(vulkan::GpuImage& img);

  void destroyBuffer(vulkan::GpuBuffer& buf);

  void destroySwapchain();

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  void bindAndroidWindow(ANativeWindow* window);

#endif
  void destroy();

  VkResult makeBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memFlags,
    vulkan::GpuBuffer& out
  );

  VkResult makeImage(
    int w,
    int h,
    VkImageUsageFlags usage,
    vulkan::GpuImage& out,
    bool sampled,
    VkFormat imgFormat = VK_FORMAT_UNDEFINED
  );

  VkRenderPass makeRenderPass(
    VkAttachmentLoadOp load, VkAttachmentStoreOp store, VkImageLayout initial
  );

  bool pickFormat();

  VkExtent2D chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& caps) const;

  VkResult recreateSwapchain();

  vulkan::GpuImage* imageFromView(void* view);
};

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
std::vector<Device::Impl*>& androidDevices();
void androidUnregisterDevice(void* impl);
#endif

struct Pass::Impl {
  Device::Impl* device = nullptr;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  bool ended = false;
  vulkan::GpuImage* target = nullptr;
  std::vector<uint8_t> bytes0;
  std::vector<uint8_t> bytes1;
  void* fragmentViews[glim::gpu::kMaxFragmentTextures]{};
  void* fragmentSamplers[glim::gpu::kMaxFragmentSamplers]{};
  VkPipeline pipeline = VK_NULL_HANDLE;
};

struct CommandEncoder::Impl {
  Device::Impl* device = nullptr;
  uint32_t frame = 0;
  uint32_t imageIndex = 0;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  bool recording = false;
  bool presentQueued = false;
};

}  // namespace glim::gpu
