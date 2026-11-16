#include <dlfcn.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#if defined(__ANDROID__)
#include <android/hardware_buffer.h>
#else
#include <unistd.h>

#include "glim/gpu/Types.h"
#endif

#include "VideoClip.h"
#include "VideoSource.h"

namespace glim {
namespace toolkit {
namespace {

constexpr std::uint32_t kVertexSpirv[] = {
#include "clip.vert.inc"
};
constexpr std::uint32_t kFragmentSpirv[] = {
#include "clip.frag.inc"
};

#define GLIM_VK_INSTANCE_FNS(X)               \
  X(vkDestroyInstance)                        \
  X(vkEnumeratePhysicalDevices)               \
  X(vkGetPhysicalDeviceQueueFamilyProperties) \
  X(vkGetPhysicalDeviceMemoryProperties)      \
  X(vkEnumerateDeviceExtensionProperties)     \
  X(vkCreateDevice)                           \
  X(vkGetDeviceProcAddr)

#if defined(__ANDROID__)
#define GLIM_VK_BUFFER_FNS(X) \
  X(vkGetAndroidHardwareBufferPropertiesANDROID)
#else
#define GLIM_VK_BUFFER_FNS(X)     \
  X(vkGetMemoryFdKHR)             \
  X(vkGetImageMemoryRequirements) \
  X(vkGetImageSubresourceLayout)
#endif

#define GLIM_VK_DEVICE_FNS(X)  \
  GLIM_VK_BUFFER_FNS(X)        \
  X(vkDestroyDevice)           \
  X(vkGetDeviceQueue)          \
  X(vkCreateImage)             \
  X(vkDestroyImage)            \
  X(vkAllocateMemory)          \
  X(vkFreeMemory)              \
  X(vkBindImageMemory)         \
  X(vkCreateImageView)         \
  X(vkDestroyImageView)        \
  X(vkCreateRenderPass)        \
  X(vkDestroyRenderPass)       \
  X(vkCreateFramebuffer)       \
  X(vkDestroyFramebuffer)      \
  X(vkCreateShaderModule)      \
  X(vkDestroyShaderModule)     \
  X(vkCreatePipelineLayout)    \
  X(vkDestroyPipelineLayout)   \
  X(vkCreateGraphicsPipelines) \
  X(vkDestroyPipeline)         \
  X(vkCreateCommandPool)       \
  X(vkDestroyCommandPool)      \
  X(vkAllocateCommandBuffers)  \
  X(vkCreateFence)             \
  X(vkDestroyFence)            \
  X(vkWaitForFences)           \
  X(vkResetFences)             \
  X(vkGetFenceStatus)          \
  X(vkBeginCommandBuffer)      \
  X(vkEndCommandBuffer)        \
  X(vkCmdPipelineBarrier)      \
  X(vkCmdBeginRenderPass)      \
  X(vkCmdEndRenderPass)        \
  X(vkCmdBindPipeline)         \
  X(vkCmdPushConstants)        \
  X(vkCmdDraw)                 \
  X(vkQueueSubmit)             \
  X(vkDeviceWaitIdle)

#define GLIM_VK_DECLARE(name) PFN_##name name = nullptr;

// The producer's own Vulkan, loaded from libvulkan here: the renderer's
// function table belongs to its device, and a producer stands apart
// from it.
struct Vk {
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
  PFN_vkCreateInstance vkCreateInstance = nullptr;
  GLIM_VK_INSTANCE_FNS(GLIM_VK_DECLARE)
  GLIM_VK_DEVICE_FNS(GLIM_VK_DECLARE)
};

struct Push {
  float width;
  float height;
  float t;
};

struct Frame {
#if defined(__ANDROID__)
  AHardwareBuffer* buffer = nullptr;
  void* surface() { return buffer; }
#else
  gpu::DmaBuf buffer;
  void* surface() { return &buffer; }
#endif
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkFence done = VK_NULL_HANDLE;
};

struct Pending {
  std::uint64_t fence;
  std::size_t slot;
};

bool hasExtension(
  const std::vector<VkExtensionProperties>& all, const char* name
) {
  for (const VkExtensionProperties& e : all) {
    if (std::strcmp(e.extensionName, name) == 0) return true;
  }
  return false;
}

#if defined(__ANDROID__)
#define GLIM_VIDEO_AVAILABILITY \
  __attribute__((availability(android, introduced = 26)))
constexpr const char* kVulkanLibrary = "libvulkan.so";
constexpr const char* kBufferExtension =
  VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME;
#else
#define GLIM_VIDEO_AVAILABILITY
constexpr const char* kVulkanLibrary = "libvulkan.so.1";
constexpr const char* kBufferExtension =
  VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME;
#endif

// Like a decoder or camera pipeline: its own device and queue writing
// into buffers the renderer imports (AHardwareBuffers on Android,
// dmabufs on Linux). Each frame's VkFence is the feed fence; buffers go
// back to the foreign queue after every write.
class GLIM_VIDEO_AVAILABILITY VulkanVideoSource final
    : public VideoSource {
 public:
  VulkanVideoSource() : feed_([this] { return signaled(); }) {
#if defined(__ANDROID__)
    const AHardwareBuffer_Desc desc{
      .width = kClipWidth,
      .height = kClipHeight,
      .layers = 1,
      .format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
      .usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
               AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT,
    };
    for (Frame& f : frames_) {
      if (AHardwareBuffer_allocate(&desc, &f.buffer) != 0)
        f.buffer = nullptr;
    }
#endif
  }

  ~VulkanVideoSource() override {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
    tearDown();
    for (Frame& f : frames_) {
#if defined(__ANDROID__)
      if (f.buffer) AHardwareBuffer_release(f.buffer);
#else
      if (f.buffer.fd >= 0) close(f.buffer.fd);
#endif
    }
    if (library_) dlclose(library_);
  }

  bool ok() {
#if defined(__ANDROID__)
    for (const Frame& f : frames_) {
      if (!f.buffer) return false;
    }
#endif
    return setUp();
  }

  void attach(paint::Context& ctx) override {
    if (attached_) return;
    attached_ = true;
    for (std::size_t i = 0; i < kClipRing; ++i) {
      ids_[i] = ctx.wrapSurface(
        frames_[i].surface(),
        kClipWidth,
        kClipHeight,
        paint::SampleFormat::Rgba8Unorm
      );
    }
    thread_ = std::thread([this] { produceLoop(); });
  }

  void setPlaying(bool playing) override {
    playing_.store(playing, std::memory_order_relaxed);
  }

  std::uint32_t acquire() override { return feed_.acquire(); }

  host::FeedStats stats() const override { return feed_.stats(); }

 private:
  std::uint64_t signaled() {
    std::lock_guard lock(mutex_);
    drainLocked();
    return reached_;
  }

  // Fence status can read out of order (lavapipe), so every signaled
  // entry goes, and all fences below the oldest unsignaled one are
  // done. A slot is never reset while its entry is still queued.
  void drainLocked() {
    std::erase_if(pending_, [&](const Pending& p) {
      const VkResult r =
        vk_.vkGetFenceStatus(device_, frames_[p.slot].done);
      static int probes = 0;
      if (++probes % 40 == 0) {
        std::fprintf(
          stderr,
          "PROBE drain fence %llu slot %zu status %d size %zu reached "
          "%llu\n",
          static_cast<unsigned long long>(p.fence),
          p.slot,
          r,
          pending_.size(),
          static_cast<unsigned long long>(reached_)
        );
      }
      return r == VK_SUCCESS;
    });
    reached_ = std::max(
      reached_,
      pending_.empty() ? submitted_ : pending_.front().fence - 1
    );
  }

  bool loadInstance() {
    library_ = dlopen(kVulkanLibrary, RTLD_NOW | RTLD_LOCAL);
    if (!library_) return false;
    vk_.vkGetInstanceProcAddr =
      reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(library_, "vkGetInstanceProcAddr")
      );
    if (!vk_.vkGetInstanceProcAddr) return false;
    vk_.vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
      vk_.vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance")
    );
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    if (!vk_.vkCreateInstance ||
        vk_.vkCreateInstance(&ici, nullptr, &instance_) != VK_SUCCESS) {
      return false;
    }
#define GLIM_VK_LOAD_INSTANCE(name)             \
  vk_.name = reinterpret_cast<PFN_##name>(      \
    vk_.vkGetInstanceProcAddr(instance_, #name) \
  );
    GLIM_VK_INSTANCE_FNS(GLIM_VK_LOAD_INSTANCE)
#undef GLIM_VK_LOAD_INSTANCE
    return true;
  }

  bool createDevice() {
    std::uint32_t count = 1;
    if (vk_.vkEnumeratePhysicalDevices(instance_, &count, &physical_) <
          0 ||
        count == 0) {
      return false;
    }
    std::uint32_t families = 0;
    vk_.vkGetPhysicalDeviceQueueFamilyProperties(
      physical_, &families, nullptr
    );
    std::vector<VkQueueFamilyProperties> props(families);
    vk_.vkGetPhysicalDeviceQueueFamilyProperties(
      physical_, &families, props.data()
    );
    family_ = families;
    for (std::uint32_t i = 0; i < families; ++i) {
      if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        family_ = i;
        break;
      }
    }
    if (family_ == families) return false;
    std::uint32_t extCount = 0;
    vk_.vkEnumerateDeviceExtensionProperties(
      physical_, nullptr, &extCount, nullptr
    );
    std::vector<VkExtensionProperties> available(extCount);
    vk_.vkEnumerateDeviceExtensionProperties(
      physical_, nullptr, &extCount, available.data()
    );
    const char* const wanted[] = {
      VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
      VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
#if !defined(__ANDROID__)
      VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
#endif
      kBufferExtension,
    };
    std::vector<const char*> exts;
    for (const char* e : wanted) {
      if (hasExtension(available, e)) {
        exts.push_back(e);
      } else {
        std::fprintf(stderr, "video producer: %s missing\n", e);
      }
    }
    if (exts.empty() || exts.back() != kBufferExtension) return false;
    if (hasExtension(
          available, VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME
        )) {
      foreign_ = VK_QUEUE_FAMILY_FOREIGN_EXT;
    }
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = family_;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = static_cast<std::uint32_t>(exts.size());
    dci.ppEnabledExtensionNames = exts.data();
    if (vk_.vkCreateDevice(physical_, &dci, nullptr, &device_) !=
        VK_SUCCESS)
      return false;
#define GLIM_VK_LOAD_DEVICE(name)           \
  vk_.name = reinterpret_cast<PFN_##name>(  \
    vk_.vkGetDeviceProcAddr(device_, #name) \
  );
    GLIM_VK_DEVICE_FNS(GLIM_VK_LOAD_DEVICE)
#undef GLIM_VK_LOAD_DEVICE
    vk_.vkGetDeviceQueue(device_, family_, 0, &queue_);
    return true;
  }

  std::uint32_t memoryType(std::uint32_t bits) const {
    VkPhysicalDeviceMemoryProperties mp{};
    vk_.vkGetPhysicalDeviceMemoryProperties(physical_, &mp);
    for (std::uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
      if (bits & (1u << i)) return i;
    }
    return UINT32_MAX;
  }

#if defined(__ANDROID__)
  bool bindMemory(Frame& f) {
    VkAndroidHardwareBufferFormatPropertiesANDROID format{};
    format.sType =
      VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;
    VkAndroidHardwareBufferPropertiesANDROID props{};
    props.sType =
      VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
    props.pNext = &format;
    if (vk_.vkGetAndroidHardwareBufferPropertiesANDROID(
          device_, f.buffer, &props
        ) != VK_SUCCESS ||
        format.format != VK_FORMAT_R8G8B8A8_UNORM) {
      return false;
    }
    VkExternalMemoryImageCreateInfo external{};
    external.sType =
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    external.handleTypes =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;
    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.pNext = &external;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent = {kClipWidth, kClipHeight, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vk_.vkCreateImage(device_, &ici, nullptr, &f.image) !=
        VK_SUCCESS)
      return false;
    VkImportAndroidHardwareBufferInfoANDROID import{};
    import.sType =
      VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    import.buffer = f.buffer;
    VkMemoryDedicatedAllocateInfo dedicated{};
    dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicated.pNext = &import;
    dedicated.image = f.image;
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = &dedicated;
    mai.allocationSize = props.allocationSize;
    mai.memoryTypeIndex = memoryType(props.memoryTypeBits);
    return mai.memoryTypeIndex != UINT32_MAX &&
           vk_.vkAllocateMemory(device_, &mai, nullptr, &f.memory) ==
             VK_SUCCESS &&
           vk_.vkBindImageMemory(device_, f.image, f.memory, 0) ==
             VK_SUCCESS;
  }
#else
  // Linear and exportable, like a decoder handing frames to a
  // compositor.
  bool bindMemory(Frame& f) {
    VkExternalMemoryImageCreateInfo external{};
    external.sType =
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    external.handleTypes =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.pNext = &external;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent = {kClipWidth, kClipHeight, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_LINEAR;
    ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!step(
          vk_.vkCreateImage(device_, &ici, nullptr, &f.image) ==
            VK_SUCCESS,
          "linear dmabuf image"
        ))
      return false;
    VkMemoryRequirements req{};
    vk_.vkGetImageMemoryRequirements(device_, f.image, &req);
    VkExportMemoryAllocateInfo exportInfo{};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportInfo.handleTypes =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    VkMemoryDedicatedAllocateInfo dedicated{};
    dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicated.pNext = &exportInfo;
    dedicated.image = f.image;
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = &dedicated;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = memoryType(req.memoryTypeBits);
    if (!step(
          mai.memoryTypeIndex != UINT32_MAX &&
            vk_.vkAllocateMemory(device_, &mai, nullptr, &f.memory) ==
              VK_SUCCESS &&
            vk_.vkBindImageMemory(device_, f.image, f.memory, 0) ==
              VK_SUCCESS,
          "exportable memory"
        ))
      return false;
    VkMemoryGetFdInfoKHR get{};
    get.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    get.memory = f.memory;
    get.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    if (!step(
          vk_.vkGetMemoryFdKHR(device_, &get, &f.buffer.fd) ==
            VK_SUCCESS,
          "dmabuf export"
        ))
      return false;
    const VkImageSubresource sub{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    VkSubresourceLayout layout{};
    vk_.vkGetImageSubresourceLayout(device_, f.image, &sub, &layout);
    f.buffer.stride = static_cast<std::uint32_t>(layout.rowPitch);
    f.buffer.offset = static_cast<std::uint32_t>(layout.offset);
    return true;
  }
#endif

  bool createFrame(Frame& f) {
    if (!bindMemory(f)) return false;
    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = f.image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = VK_FORMAT_R8G8B8A8_UNORM;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vk_.vkCreateImageView(device_, &vci, nullptr, &f.view) !=
        VK_SUCCESS)
      return false;
    VkFramebufferCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fci.renderPass = renderPass_;
    fci.attachmentCount = 1;
    fci.pAttachments = &f.view;
    fci.width = kClipWidth;
    fci.height = kClipHeight;
    fci.layers = 1;
    if (vk_.vkCreateFramebuffer(
          device_, &fci, nullptr, &f.framebuffer
        ) != VK_SUCCESS)
      return false;
    VkCommandBufferAllocateInfo cai{};
    cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cai.commandPool = pool_;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    VkFenceCreateInfo fen{};
    fen.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fen.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    return vk_.vkAllocateCommandBuffers(device_, &cai, &f.cmd) ==
             VK_SUCCESS &&
           vk_.vkCreateFence(device_, &fen, nullptr, &f.done) ==
             VK_SUCCESS;
  }

  VkShaderModule module(const std::uint32_t* code, std::size_t bytes)
    const {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = bytes;
    ci.pCode = code;
    VkShaderModule m = VK_NULL_HANDLE;
    vk_.vkCreateShaderModule(device_, &ci, nullptr, &m);
    return m;
  }

  bool createPipeline() {
    VkAttachmentDescription color{};
    color.format = VK_FORMAT_R8G8B8A8_UNORM;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentReference ref{
      0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;
    VkRenderPassCreateInfo rci{};
    rci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rci.attachmentCount = 1;
    rci.pAttachments = &color;
    rci.subpassCount = 1;
    rci.pSubpasses = &sub;
    if (vk_.vkCreateRenderPass(device_, &rci, nullptr, &renderPass_) !=
        VK_SUCCESS)
      return false;

    VkPushConstantRange range{
      VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Push)
    };
    VkPipelineLayoutCreateInfo lci{};
    lci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &range;
    if (vk_.vkCreatePipelineLayout(device_, &lci, nullptr, &layout_) !=
        VK_SUCCESS)
      return false;

    const VkShaderModule vert =
      module(kVertexSpirv, sizeof kVertexSpirv);
    const VkShaderModule frag =
      module(kFragmentSpirv, sizeof kFragmentSpirv);
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1] = stages[0];
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    const VkViewport viewport{0, 0, kClipWidth, kClipHeight, 0, 1};
    const VkRect2D scissor{{0, 0}, {kClipWidth, kClipHeight}};
    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.pViewports = &viewport;
    vp.scissorCount = 1;
    vp.pScissors = &scissor;
    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &blend;
    VkGraphicsPipelineCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pci.stageCount = 2;
    pci.pStages = stages;
    pci.pVertexInputState = &vin;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms;
    pci.pColorBlendState = &cb;
    pci.layout = layout_;
    pci.renderPass = renderPass_;
    const bool ok =
      vert && frag &&
      vk_.vkCreateGraphicsPipelines(
        device_, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline_
      ) == VK_SUCCESS;
    if (vert) vk_.vkDestroyShaderModule(device_, vert, nullptr);
    if (frag) vk_.vkDestroyShaderModule(device_, frag, nullptr);
    return ok;
  }

  static bool step(bool ok, const char* what) {
    if (!ok) std::fprintf(stderr, "video producer: %s failed\n", what);
    return ok;
  }

  bool createPool() {
    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = family_;
    return vk_.vkCreateCommandPool(device_, &pci, nullptr, &pool_) ==
           VK_SUCCESS;
  }

  bool setUp() {
    if (!step(loadInstance(), "instance") ||
        !step(createDevice(), "device") ||
        !step(createPipeline(), "pipeline") ||
        !step(createPool(), "pool"))
      return false;
    for (Frame& f : frames_) {
      if (!step(createFrame(f), "frame")) return false;
    }
    return true;
  }

  void tearDown() {
    if (device_) {
      vk_.vkDeviceWaitIdle(device_);
      for (Frame& f : frames_) {
        vk_.vkDestroyFence(device_, f.done, nullptr);
        vk_.vkDestroyFramebuffer(device_, f.framebuffer, nullptr);
        vk_.vkDestroyImageView(device_, f.view, nullptr);
        vk_.vkDestroyImage(device_, f.image, nullptr);
        vk_.vkFreeMemory(device_, f.memory, nullptr);
      }
      vk_.vkDestroyCommandPool(device_, pool_, nullptr);
      vk_.vkDestroyPipeline(device_, pipeline_, nullptr);
      vk_.vkDestroyPipelineLayout(device_, layout_, nullptr);
      vk_.vkDestroyRenderPass(device_, renderPass_, nullptr);
      vk_.vkDestroyDevice(device_, nullptr);
      device_ = VK_NULL_HANDLE;
    }
    if (instance_) {
      vk_.vkDestroyInstance(instance_, nullptr);
      instance_ = VK_NULL_HANDLE;
    }
  }

  void ownership(VkCommandBuffer cmd, VkImage image, bool acquire)
    const {
    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.srcAccessMask =
      acquire ? 0 : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    b.oldLayout =
      acquire ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
    b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    b.srcQueueFamilyIndex = acquire ? foreign_ : family_;
    b.dstQueueFamilyIndex = acquire ? family_ : foreign_;
    b.image = image;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vk_.vkCmdPipelineBarrier(
      cmd,
      acquire ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
              : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      acquire ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
              : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      0,
      0,
      nullptr,
      0,
      nullptr,
      1,
      &b
    );
  }

  void produce() {
    const std::optional<std::size_t> claimed = feed_.claim(ids_);
    if (!claimed) return;
    const std::size_t slot = *claimed;
    Frame& f = frames_[slot];
    if (!live(
          vk_.vkWaitForFences(device_, 1, &f.done, VK_TRUE, UINT64_MAX),
          "fence wait"
        ))
      return;
    std::fprintf(
      stderr,
      "PROBE waited slot %zu status %d\n",
      slot,
      vk_.vkGetFenceStatus(device_, f.done)
    );
    {
      std::lock_guard lock(mutex_);
      drainLocked();
      vk_.vkResetFences(device_, 1, &f.done);
    }
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_.vkBeginCommandBuffer(f.cmd, &bi);
    ownership(f.cmd, f.image, true);
    VkRenderPassBeginInfo rbi{};
    rbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rbi.renderPass = renderPass_;
    rbi.framebuffer = f.framebuffer;
    rbi.renderArea = {{0, 0}, {kClipWidth, kClipHeight}};
    vk_.vkCmdBeginRenderPass(f.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
    vk_.vkCmdBindPipeline(
      f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_
    );
    const Push push{
      kClipWidth,
      kClipHeight,
      static_cast<float>(static_cast<double>(fence_) / kClipFps)
    };
    vk_.vkCmdPushConstants(
      f.cmd,
      layout_,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      0,
      sizeof push,
      &push
    );
    vk_.vkCmdDraw(f.cmd, 3, 1, 0, 0);
    vk_.vkCmdEndRenderPass(f.cmd);
    ownership(f.cmd, f.image, false);
    vk_.vkEndCommandBuffer(f.cmd);
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &f.cmd;
    {
      std::lock_guard lock(mutex_);
      if (!live(vk_.vkQueueSubmit(queue_, 1, &si, f.done), "submit"))
        return;
      ++fence_;
      pending_.push_back({fence_, slot});
      submitted_ = fence_;
    }
    feed_.publish({ids_[slot], fence_});
  }

  // A failed wait or submit (device lost) stops the producer for good
  // rather than publishing frames that never land.
  bool live(VkResult r, const char* what) {
    if (r == VK_SUCCESS) return true;
    std::fprintf(
      stderr, "video producer: %s returned %d; stopping\n", what, r
    );
    stop_ = true;
    return false;
  }

  void produceLoop() {
    const auto interval =
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / kClipFps)
      );
    auto next = std::chrono::steady_clock::now();
    while (!stop_) {
      if (playing_.load(std::memory_order_relaxed)) produce();
      next += interval;
      std::this_thread::sleep_until(next);
    }
  }

  host::Feed feed_;
  Vk vk_;
  void* library_ = nullptr;
  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  std::uint32_t family_ = 0;
  std::uint32_t foreign_ = VK_QUEUE_FAMILY_EXTERNAL;
  VkRenderPass renderPass_ = VK_NULL_HANDLE;
  VkPipelineLayout layout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  VkCommandPool pool_ = VK_NULL_HANDLE;
  std::array<Frame, kClipRing> frames_{};
  std::array<std::uint32_t, kClipRing> ids_{};
  std::mutex mutex_;
  std::deque<Pending> pending_;
  std::uint64_t reached_ = 0;
  std::uint64_t submitted_ = 0;
  std::uint64_t fence_ = 0;
  std::thread thread_;
  std::atomic<bool> playing_{true};
  std::atomic<bool> stop_{false};
  bool attached_ = false;
};

}  // namespace

std::unique_ptr<VideoSource> makeVideoSource() {
#if defined(__ANDROID__)
  if (__builtin_available(android 26, *)) {
#else
  {
#endif
    auto source = std::make_unique<VulkanVideoSource>();
    if (source->ok()) return source;
  }
  return nullptr;
}

}  // namespace toolkit
}  // namespace glim
