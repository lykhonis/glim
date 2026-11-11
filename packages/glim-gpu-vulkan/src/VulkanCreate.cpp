#include "VulkanImpl.h"

namespace glim::gpu {

using namespace vulkan;

Result<Device> Device::create(const DeviceCreateInfo& info) {
  if (info.backend != Backend::Vulkan) {
    return Result<Device>::fail("glim-gpu-vulkan: backend is not Vulkan");
  }
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  auto* androidWindow = static_cast<ANativeWindow*>(info.native[0]);
  const bool headless = (androidWindow == nullptr);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
  auto* display = static_cast<wl_display*>(info.native[0]);
  auto* wlSurface = static_cast<wl_surface*>(info.native[1]);
  const bool headless = (display == nullptr && wlSurface == nullptr);
  if (!headless && (!display || !wlSurface)) {
    return Result<Device>::fail("Vulkan Wayland display/surface is null");
  }
#else
  return Result<Device>::fail("Vulkan WSI platform is not enabled");
#endif
  if (volkInitialize() != VK_SUCCESS) {
    return Result<Device>::fail("volkInitialize failed (libvulkan.so missing?)");
  }

  Device out;
  out.impl_ = std::make_unique<Impl>();
  Impl& d = *out.impl_;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  d.androidWindow = androidWindow;
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
  d.display = display;
  d.wlSurface = wlSurface;
#endif
  d.vsync = info.native[2] != nullptr;
  d.isHeadless = headless;
  d.shaders.resize(1);
  d.pipelines.resize(1);
  d.buffers.resize(1);
  d.targets.resize(1);

  uint32_t instExtCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &instExtCount, nullptr);
  std::vector<VkExtensionProperties> instExt(instExtCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &instExtCount, instExt.data());
  std::vector<const char*> instanceExts;
  if (!headless) {
    instanceExts = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
#endif
    };
  }
  for (const char* e : instanceExts) {
    if (!hasExtension(instExt, e)) {
      return Result<Device>::fail(std::string("missing instance extension ") + e);
    }
  }

  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "glim";
  app.apiVersion = VK_API_VERSION_1_1;

  VkInstanceCreateInfo ici{};
  ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ici.pApplicationInfo = &app;
  ici.enabledExtensionCount = static_cast<uint32_t>(instanceExts.size());
  ici.ppEnabledExtensionNames = instanceExts.data();
#if GLIM_DEBUG
  const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
  uint32_t layerCount = 0;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> layerProps(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, layerProps.data());
  bool haveValidation = false;
  for (const auto& lp : layerProps) {
    if (std::strcmp(lp.layerName, layers[0]) == 0) {
      haveValidation = true;
      break;
    }
  }
  if (haveValidation) {
    ici.enabledLayerCount = 1;
    ici.ppEnabledLayerNames = layers;
  }
#endif
  if (vkCreateInstance(&ici, nullptr, &d.instance) != VK_SUCCESS) {
    return Result<Device>::fail("vkCreateInstance failed");
  }
  volkLoadInstance(d.instance);

  if (!headless) {
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    VkAndroidSurfaceCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    sci.window = androidWindow;
    if (vkCreateAndroidSurfaceKHR(d.instance, &sci, nullptr, &d.surface) != VK_SUCCESS) {
      return Result<Device>::fail("vkCreateAndroidSurfaceKHR failed");
    }
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    VkWaylandSurfaceCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    sci.display = display;
    sci.surface = wlSurface;
    if (vkCreateWaylandSurfaceKHR(d.instance, &sci, nullptr, &d.surface) != VK_SUCCESS) {
      return Result<Device>::fail("vkCreateWaylandSurfaceKHR failed");
    }
#endif
  }

  uint32_t physCount = 0;
  vkEnumeratePhysicalDevices(d.instance, &physCount, nullptr);
  std::vector<VkPhysicalDevice> phys(physCount);
  vkEnumeratePhysicalDevices(d.instance, &physCount, phys.data());
  if (phys.empty()) {
    return Result<Device>::fail("no Vulkan physical device");
  }

  auto scoreDevice = [&](VkPhysicalDevice p, uint32_t* familyOut) -> int {
    uint32_t n = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(p, &n, nullptr);
    std::vector<VkQueueFamilyProperties> qf(n);
    vkGetPhysicalDeviceQueueFamilyProperties(p, &n, qf.data());
    int fam = -1;
    for (uint32_t i = 0; i < n; ++i) {
      if (!(qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
        continue;
      }
      if (headless) {
        fam = static_cast<int>(i);
        break;
      }
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
      if (!vkGetPhysicalDeviceWaylandPresentationSupportKHR(p, i, display)) {
        continue;
      }
#endif
      VkBool32 present = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(p, i, d.surface, &present);
      if (present) {
        fam = static_cast<int>(i);
        break;
      }
    }
    if (fam < 0) {
      return -1;
    }
    uint32_t extN = 0;
    vkEnumerateDeviceExtensionProperties(p, nullptr, &extN, nullptr);
    std::vector<VkExtensionProperties> dext(extN);
    vkEnumerateDeviceExtensionProperties(p, nullptr, &extN, dext.data());
    if (!headless && !hasExtension(dext, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
      return -1;
    }
    *familyOut = static_cast<uint32_t>(fam);
    VkPhysicalDeviceProperties pp{};
    vkGetPhysicalDeviceProperties(p, &pp);
    int score = 1;
    if (pp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      score += 100;
    } else if (pp.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
      score += 10;
    }
    return score;
  };

  int best = -1;
  for (VkPhysicalDevice p : phys) {
    uint32_t fam = 0;
    const int s = scoreDevice(p, &fam);
    if (s > best) {
      best = s;
      d.physical = p;
      d.queueFamily = fam;
    }
  }
  if (best < 0) {
    return Result<Device>::fail(
        headless ? "no GPU with graphics queue" : "no GPU with graphics + present + swapchain"
    );
  }
  vkGetPhysicalDeviceProperties(d.physical, &d.props);
  d.ringAlign = std::max<VkDeviceSize>(d.props.limits.minStorageBufferOffsetAlignment, 16);

  float prio = 1.0f;
  VkDeviceQueueCreateInfo qci{};
  qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  qci.queueFamilyIndex = d.queueFamily;
  qci.queueCount = 1;
  qci.pQueuePriorities = &prio;
  uint32_t extN = 0;
  vkEnumerateDeviceExtensionProperties(d.physical, nullptr, &extN, nullptr);
  std::vector<VkExtensionProperties> dext(extN);
  vkEnumerateDeviceExtensionProperties(d.physical, nullptr, &extN, dext.data());
  std::vector<const char*> devExts;
  if (!headless) {
    devExts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  const char* ahbExts[] = {
      VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
      VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME
  };
  d.haveAhb = true;
  for (const char* e : ahbExts) {
    if (!hasExtension(dext, e)) {
      d.haveAhb = false;
      break;
    }
    devExts.push_back(e);
  }
  if (!d.haveAhb) {
    devExts.resize(headless ? 0 : 1);
  }
#else
  (void)dext;
#endif
  VkDeviceCreateInfo dci{};
  dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  dci.enabledExtensionCount = static_cast<uint32_t>(devExts.size());
  dci.ppEnabledExtensionNames = devExts.data();
  if (vkCreateDevice(d.physical, &dci, nullptr, &d.device) != VK_SUCCESS) {
    return Result<Device>::fail("vkCreateDevice failed");
  }
  volkLoadDevice(d.device);
  vkGetDeviceQueue(d.device, d.queueFamily, 0, &d.queue);
  d.queueWrapper = Queue(d.queue);

  if (!headless && !d.pickFormat()) {
    return Result<Device>::fail("no suitable swapchain format");
  }
  d.rpClear = d.makeRenderPass(
      VK_ATTACHMENT_LOAD_OP_CLEAR,
      VK_ATTACHMENT_STORE_OP_STORE,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  );
  d.rpLoad = d.makeRenderPass(
      VK_ATTACHMENT_LOAD_OP_LOAD,
      VK_ATTACHMENT_STORE_OP_STORE,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  );
  d.rpDontCare = d.makeRenderPass(
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VK_ATTACHMENT_STORE_OP_STORE,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  );
  if (!d.rpClear || !d.rpLoad || !d.rpDontCare) {
    return Result<Device>::fail("vkCreateRenderPass failed");
  }

  VkDescriptorSetLayoutBinding binds[kBindingCount]{};
  binds[0].binding = 0;
  binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  binds[0].descriptorCount = 1;
  binds[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  binds[1].binding = 1;
  binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  binds[1].descriptorCount = 1;
  binds[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  for (int i = 0; i < kMaxFragmentTextures; ++i) {
    binds[2 + i].binding = static_cast<uint32_t>(2 + i);
    binds[2 + i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    binds[2 + i].descriptorCount = 1;
    binds[2 + i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  for (int i = 0; i < kMaxFragmentSamplers; ++i) {
    VkDescriptorSetLayoutBinding& b = binds[2 + kMaxFragmentTextures + i];
    b.binding = static_cast<uint32_t>(kSamplerBinding + i);
    b.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  VkDescriptorSetLayoutCreateInfo sl{};
  sl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  sl.bindingCount = kBindingCount;
  sl.pBindings = binds;
  if (vkCreateDescriptorSetLayout(d.device, &sl, nullptr, &d.setLayout) != VK_SUCCESS) {
    return Result<Device>::fail("vkCreateDescriptorSetLayout failed");
  }
  VkPipelineLayoutCreateInfo pl{};
  pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pl.setLayoutCount = 1;
  pl.pSetLayouts = &d.setLayout;
  if (vkCreatePipelineLayout(d.device, &pl, nullptr, &d.pipelineLayout) != VK_SUCCESS) {
    return Result<Device>::fail("vkCreatePipelineLayout failed");
  }

  VkSamplerCreateInfo samp{};
  samp.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samp.magFilter = VK_FILTER_LINEAR;
  samp.minFilter = VK_FILTER_LINEAR;
  samp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samp.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samp.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samp.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  if (vkCreateSampler(d.device, &samp, nullptr, &d.sampler) != VK_SUCCESS) {
    return Result<Device>::fail("vkCreateSampler failed");
  }

  if (d.makeImage(
          1,
          1,
          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
              VK_IMAGE_USAGE_TRANSFER_DST_BIT,
          d.dummyImage,
          true
      ) != VK_SUCCESS) {
    return Result<Device>::fail("dummy image failed");
  }
  if (d.makeBuffer(
          kRingBytes * kFrames,
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
          d.ring
      ) != VK_SUCCESS ||
      !d.ring.mapped) {
    return Result<Device>::fail("upload ring buffer failed");
  }

  VkCommandPoolCreateInfo cp{};
  cp.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cp.queueFamilyIndex = d.queueFamily;
  cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  if (vkCreateCommandPool(d.device, &cp, nullptr, &d.cmdPool) != VK_SUCCESS) {
    return Result<Device>::fail("vkCreateCommandPool failed");
  }
  cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  if (vkCreateCommandPool(d.device, &cp, nullptr, &d.uploadPool) != VK_SUCCESS) {
    return Result<Device>::fail("vkCreateCommandPool failed");
  }

  VkCommandBufferAllocateInfo cba{};
  cba.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cba.commandPool = d.cmdPool;
  cba.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cba.commandBufferCount = kFrames;
  VkCommandBuffer cmds[kFrames]{};
  vkAllocateCommandBuffers(d.device, &cba, cmds);

  constexpr uint32_t kMaxSetsPerFrame = 4096;
  VkDescriptorPoolSize poolSizes[3]{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolSizes[0].descriptorCount = kMaxSetsPerFrame * 2;
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  poolSizes[1].descriptorCount = kMaxSetsPerFrame * kMaxFragmentTextures;
  poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
  poolSizes[2].descriptorCount = kMaxSetsPerFrame * kMaxFragmentSamplers;
  VkDescriptorPoolCreateInfo dpi{};
  dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  dpi.maxSets = kMaxSetsPerFrame;
  dpi.poolSizeCount = 3;
  dpi.pPoolSizes = poolSizes;

  VkFenceCreateInfo fi{};
  fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  VkSemaphoreCreateInfo sei{};
  sei.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  for (uint32_t i = 0; i < kFrames; ++i) {
    d.frames[i].cmd = cmds[i];
    vkCreateFence(d.device, &fi, nullptr, &d.frames[i].inFlight);
    vkCreateSemaphore(d.device, &sei, nullptr, &d.frames[i].imageAvailable);
    vkCreateSemaphore(d.device, &sei, nullptr, &d.frames[i].renderFinished);
    vkCreateDescriptorPool(d.device, &dpi, nullptr, &d.frames[i].descriptors);
  }

  vkResetFences(d.device, 1, &d.frames[0].inFlight);
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(d.frames[0].cmd, &begin);
  imageBarrier(
      d.frames[0].cmd,
      d.dummyImage.image,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      0,
      VK_ACCESS_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
  );
  vkEndCommandBuffer(d.frames[0].cmd);
  VkSubmitInfo dummySubmit{};
  dummySubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  dummySubmit.commandBufferCount = 1;
  dummySubmit.pCommandBuffers = &d.frames[0].cmd;
  vkQueueSubmit(d.queue, 1, &dummySubmit, d.frames[0].inFlight);
  vkWaitForFences(d.device, 1, &d.frames[0].inFlight, VK_TRUE, UINT64_MAX);
  d.dummyImage.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  androidDevices().push_back(&d);
#endif
  return Result<Device>::ok(std::move(out));
}

}  // namespace glim::gpu
