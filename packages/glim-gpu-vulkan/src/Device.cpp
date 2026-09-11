#include <glim/gpu/Device.h>

#include <volk.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#include <android/native_window.h>
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <wayland-client.h>
#endif

namespace glim::gpu {
namespace {

constexpr uint32_t kFrames = 2;
constexpr VkDeviceSize kRingBytes = 2 * 1024 * 1024;

VkDeviceSize alignUp64(VkDeviceSize v, VkDeviceSize a) {
    return a ? (v + a - 1) & ~(a - 1) : v;
}

VkBlendFactor dstFactor(Blend blend) {
    return blend == Blend::Plus ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
}

uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t filter, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(phys, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((filter & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool hasExtension(const std::vector<VkExtensionProperties>& ext, const char* name) {
    for (const auto& e : ext) {
        if (std::strcmp(e.extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

void imageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                  VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage,
                  VkPipelineStageFlags dstStage) {
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

}  // namespace

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
void androidUnregisterDevice(void* impl);
#endif

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

struct Device::Impl {
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    ANativeWindow* androidWindow = nullptr;
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    wl_display* display = nullptr;
    wl_surface* wlSurface = nullptr;
#endif
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    Queue queueWrapper;
    bool vsync = true;

    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D extent{1, 1};
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<GpuImage> swapImages;
    uint32_t imageIndex = 0;
    uint32_t frameIndex = 0;
    bool haveSwapchain = false;

    VkRenderPass rpClear = VK_NULL_HANDLE;
    VkRenderPass rpLoad = VK_NULL_HANDLE;
    VkRenderPass rpDontCare = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    GpuImage dummyImage{};
    GpuBuffer ring{};
    VkDeviceSize ringAlign = 256;
    VkCommandPool cmdPool = VK_NULL_HANDLE;
    Frame frames[kFrames]{};

    std::vector<ShaderRec> shaders;
    std::vector<PipelineRec> pipelines;
    std::vector<GpuBuffer> buffers;
    std::vector<GpuImage> targets;
    Handle next = 1;

    VkPhysicalDeviceProperties props{};

    ~Impl() { destroy(); }

    void destroyImage(GpuImage& img) {
        if (!device) {
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

    void destroyBuffer(GpuBuffer& buf) {
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

    void destroySwapchain() {
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
    void bindAndroidWindow(ANativeWindow* window) {
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

    void destroy() {
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
            if (cmdPool) {
                vkDestroyCommandPool(device, cmdPool, nullptr);
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
        sampler = VK_NULL_HANDLE;
        pipelineLayout = VK_NULL_HANDLE;
        setLayout = VK_NULL_HANDLE;
        rpClear = rpLoad = rpDontCare = VK_NULL_HANDLE;
        for (Frame& f : frames) {
            f = {};
        }
    }

    VkResult makeBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memFlags,
                        GpuBuffer& out) {
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

    VkResult makeImage(int w, int h, VkImageUsageFlags usage, GpuImage& out, bool sampled) {
        destroyImage(out);
        out.width = std::max(w, 1);
        out.height = std::max(h, 1);
        VkImageCreateInfo ii{};
        ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.format = format;
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
        vi.format = format;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        r = vkCreateImageView(device, &vi, nullptr, &out.view);
        if (r != VK_SUCCESS) {
            return r;
        }
        if (rpLoad) {
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

    VkRenderPass makeRenderPass(VkAttachmentLoadOp load, VkAttachmentStoreOp store,
                                VkImageLayout initial) {
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
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
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

    bool pickFormat() {
        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, formats.data());
        if (formats.empty()) {
            return false;
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

    VkResult recreateSwapchain() {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);
        if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
            return VK_NOT_READY;
        }
        if (caps.currentExtent.width != UINT32_MAX) {
            extent = caps.currentExtent;
        } else {
            extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height =
                std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }
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
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.preTransform = caps.currentTransform;
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

    GpuImage* imageFromView(void* view) {
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
};

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

struct Pass::Impl {
    Device::Impl* device = nullptr;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    bool ended = false;
    GpuImage* target = nullptr;
    std::vector<uint8_t> bytes0;
    std::vector<uint8_t> bytes1;
    void* fragmentView = nullptr;
    void* fragmentSampler = nullptr;
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

Queue::Queue() = default;
Queue::Queue(void* native) : native_(native) {}

Pass::Pass() : impl_(std::make_unique<Impl>()) {}
Pass::~Pass() {
    if (impl_ && impl_->cmd && !impl_->ended) {
        vkCmdEndRenderPass(impl_->cmd);
        if (impl_->target && !impl_->target->swapchain) {
            imageBarrier(impl_->cmd, impl_->target->image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
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

void Pass::setFragmentTexture(std::uint32_t, void* nativeTexture) {
    impl_->fragmentView = nativeTexture;
}

void Pass::setFragmentSampler(std::uint32_t, void* nativeSampler) {
    impl_->fragmentSampler = nativeSampler;
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

void Pass::draw(std::uint32_t vertexCount, std::uint32_t instanceCount, std::uint32_t firstVertex,
                std::uint32_t firstInstance) {
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
    const VkDeviceSize off1 = upload(impl_->bytes1, d->props.limits.minUniformBufferOffsetAlignment);

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = frame.descriptors;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &d->setLayout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(d->device, &ai, &set) != VK_SUCCESS) {
        vkResetDescriptorPool(d->device, frame.descriptors, 0);
        if (vkAllocateDescriptorSets(d->device, &ai, &set) != VK_SUCCESS) {
            return;
        }
    }

    VkDescriptorBufferInfo ssbo{};
    ssbo.buffer = d->ring.buffer;
    ssbo.offset = impl_->bytes0.empty() ? 0 : off0;
    ssbo.range = impl_->bytes0.empty() ? 16 : impl_->bytes0.size();
    VkDescriptorBufferInfo ubo{};
    ubo.buffer = d->ring.buffer;
    ubo.offset = impl_->bytes1.empty() ? 0 : off1;
    ubo.range = impl_->bytes1.empty() ? 64 : impl_->bytes1.size();
    VkDescriptorImageInfo image{};
    image.sampler =
        impl_->fragmentSampler ? reinterpret_cast<VkSampler>(impl_->fragmentSampler) : d->sampler;
    image.imageView =
        impl_->fragmentView ? reinterpret_cast<VkImageView>(impl_->fragmentView) : d->dummyImage.view;
    image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[3]{};
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
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].pBufferInfo = &ubo;
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = set;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].pImageInfo = &image;
    vkUpdateDescriptorSets(d->device, 3, writes, 0, nullptr);
    vkCmdBindDescriptorSets(impl_->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, d->pipelineLayout, 0, 1, &set,
                            0, nullptr);
    vkCmdDraw(impl_->cmd, vertexCount, instanceCount, firstVertex, firstInstance);
}

void Pass::drawIndexed(std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t) {}

void Pass::end() {
    if (!impl_ || impl_->ended || !impl_->cmd) {
        return;
    }
    vkCmdEndRenderPass(impl_->cmd);
    if (impl_->target && !impl_->target->swapchain) {
        imageBarrier(impl_->cmd, impl_->target->image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
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
        imageBarrier(impl_->cmd, img->image, img->layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, src,
                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, srcStage,
                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
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

void CommandEncoder::present(const Drawable&) {
    impl_->presentQueued = true;
}

void CommandEncoder::submit(Queue&) {
    Device::Impl* d = impl_->device;
    if (!d || !impl_->recording) {
        return;
    }
    Frame& frame = d->frames[impl_->frame];
    if (impl_->presentQueued && impl_->imageIndex < d->swapImages.size()) {
        GpuImage& img = d->swapImages[impl_->imageIndex];
        if (img.layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
            imageBarrier(impl_->cmd, img.image, img.layout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
            img.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }
    }
    vkEndCommandBuffer(impl_->cmd);
    impl_->recording = false;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &frame.imageAvailable;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &impl_->cmd;
    si.signalSemaphoreCount = 1;
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
    }
}

Device::Device() = default;
Device::~Device() = default;
Device::Device(Device&&) noexcept = default;
Device& Device::operator=(Device&&) noexcept = default;

Result<Device> Device::create(const DeviceCreateInfo& info) {
    if (info.backend != Backend::Vulkan) {
        return Result<Device>::fail("glim-gpu-vulkan: backend is not Vulkan");
    }
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    auto* androidWindow = static_cast<ANativeWindow*>(info.native[0]);
    if (!androidWindow) {
        return Result<Device>::fail("Vulkan Android ANativeWindow is null");
    }
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    auto* display = static_cast<wl_display*>(info.native[0]);
    auto* wlSurface = static_cast<wl_surface*>(info.native[1]);
    if (!display || !wlSurface) {
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
    d.shaders.resize(1);
    d.pipelines.resize(1);
    d.buffers.resize(1);
    d.targets.resize(1);

    uint32_t instExtCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &instExtCount, nullptr);
    std::vector<VkExtensionProperties> instExt(instExtCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &instExtCount, instExt.data());
    std::vector<const char*> instanceExts = {VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
                                             VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
                                             VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
#endif
    };
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
        if (!hasExtension(dext, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
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
        return Result<Device>::fail("no GPU with graphics + present + swapchain");
    }
    vkGetPhysicalDeviceProperties(d.physical, &d.props);
    d.ringAlign = std::max<VkDeviceSize>(d.props.limits.minStorageBufferOffsetAlignment, 16);

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = d.queueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    const char* devExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = devExts;
    if (vkCreateDevice(d.physical, &dci, nullptr, &d.device) != VK_SUCCESS) {
        return Result<Device>::fail("vkCreateDevice failed");
    }
    volkLoadDevice(d.device);
    vkGetDeviceQueue(d.device, d.queueFamily, 0, &d.queue);
    d.queueWrapper = Queue(d.queue);

    if (!d.pickFormat()) {
        return Result<Device>::fail("no suitable swapchain format");
    }
    d.rpClear = d.makeRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    d.rpLoad = d.makeRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    d.rpDontCare = d.makeRenderPass(VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    if (!d.rpClear || !d.rpLoad || !d.rpDontCare) {
        return Result<Device>::fail("vkCreateRenderPass failed");
    }

    VkDescriptorSetLayoutBinding binds[3]{};
    binds[0].binding = 0;
    binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binds[0].descriptorCount = 1;
    binds[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binds[1].binding = 1;
    binds[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binds[1].descriptorCount = 1;
    binds[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binds[2].binding = 2;
    binds[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binds[2].descriptorCount = 1;
    binds[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo sl{};
    sl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    sl.bindingCount = 3;
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
    samp.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samp.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samp.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(d.device, &samp, nullptr, &d.sampler) != VK_SUCCESS) {
        return Result<Device>::fail("vkCreateSampler failed");
    }

    if (d.makeImage(1, 1,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    d.dummyImage, true) != VK_SUCCESS) {
        return Result<Device>::fail("dummy image failed");
    }
    if (d.makeBuffer(kRingBytes * kFrames,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     d.ring) != VK_SUCCESS ||
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

    VkCommandBufferAllocateInfo cba{};
    cba.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cba.commandPool = d.cmdPool;
    cba.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cba.commandBufferCount = kFrames;
    VkCommandBuffer cmds[kFrames]{};
    vkAllocateCommandBuffers(d.device, &cba, cmds);

    VkDescriptorPoolSize poolSizes[3]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 256;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 256;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = 256;
    VkDescriptorPoolCreateInfo dpi{};
    dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpi.maxSets = 256;
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
    imageBarrier(d.frames[0].cmd, d.dummyImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_SHADER_READ_BIT,
                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
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

Queue& Device::queue() {
    return impl_->queueWrapper;
}

Result<Drawable> Device::nextDrawable() {
    for (GpuImage& t : impl_->targets) {
        t.inUse = false;
    }
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    if (!impl_->surface) {
        return Result<Drawable>::fail("Android window is gone");
    }
#endif
    if (!impl_->haveSwapchain) {
        const VkResult r = impl_->recreateSwapchain();
        if (r != VK_SUCCESS) {
            return Result<Drawable>::fail("swapchain create failed");
        }
    }
    impl_->frameIndex = (impl_->frameIndex + 1) % kFrames;
    Frame& frame = impl_->frames[impl_->frameIndex];
    vkWaitForFences(impl_->device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(impl_->device, 1, &frame.inFlight);
    vkResetDescriptorPool(impl_->device, frame.descriptors, 0);
    frame.ringOffset = static_cast<VkDeviceSize>(impl_->frameIndex) * kRingBytes;

    uint32_t index = 0;
    VkResult r = vkAcquireNextImageKHR(impl_->device, impl_->swapchain, UINT64_MAX, frame.imageAvailable,
                                       VK_NULL_HANDLE, &index);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        if (impl_->recreateSwapchain() != VK_SUCCESS) {
            return Result<Drawable>::fail("swapchain recreate failed");
        }
        r = vkAcquireNextImageKHR(impl_->device, impl_->swapchain, UINT64_MAX, frame.imageAvailable,
                                  VK_NULL_HANDLE, &index);
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

Result<FrameTarget> Device::createFrameTarget(const FrameTargetDesc& desc) {
    const int w = std::max(desc.width, 1);
    const int h = std::max(desc.height, 1);
    GpuImage* found = nullptr;
    for (std::size_t i = 1; i < impl_->targets.size(); ++i) {
        GpuImage& img = impl_->targets[i];
        if (!img.inUse && img.image && img.width == w && img.height == h) {
            found = &img;
            break;
        }
    }
    Handle handle = 0;
    if (!found) {
        handle = impl_->next++;
        if (handle >= impl_->targets.size()) {
            impl_->targets.resize(handle + 1);
        }
        GpuImage& img = impl_->targets[handle];
        if (impl_->makeImage(w, h,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                             img, true) != VK_SUCCESS) {
            return Result<FrameTarget>::fail("Vulkan FrameTarget image failed");
        }
        found = &img;
    } else {
        handle = static_cast<Handle>(found - impl_->targets.data());
    }
    found->inUse = true;
    FrameTarget t;
    t.handle_ = handle;
    t.width_ = w;
    t.height_ = h;
    t.native_ = found->view;
    return Result<FrameTarget>::ok(t);
}

Result<Buffer> Device::createBuffer(const BufferDesc& desc) {
    GpuBuffer buf{};
    if (impl_->makeBuffer(desc.size,
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                              VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          buf) != VK_SUCCESS) {
        return Result<Buffer>::fail("vkCreateBuffer failed");
    }
    Buffer b;
    b.handle_ = impl_->next++;
    if (b.handle_ >= impl_->buffers.size()) {
        impl_->buffers.resize(b.handle_ + 1);
    }
    impl_->buffers[b.handle_] = buf;
    b.native_ = impl_->buffers[b.handle_].buffer;
    b.size_ = impl_->buffers[b.handle_].size;
    return Result<Buffer>::ok(b);
}

void Device::writeBuffer(Buffer& buffer, const void* data, std::uint64_t size) {
    if (buffer.handle_ >= impl_->buffers.size() || !data) {
        return;
    }
    GpuBuffer& buf = impl_->buffers[buffer.handle_];
    if (!buf.mapped) {
        return;
    }
    std::memcpy(buf.mapped, data, static_cast<size_t>(std::min<std::uint64_t>(size, buf.size)));
}

Result<Texture> Device::createTexture(const TextureDesc&) {
    Texture t;
    t.handle_ = impl_->next++;
    return Result<Texture>::ok(t);
}

Result<Shader> Device::createShader(ShaderStage, const char* sourceUtf8, std::uint64_t size) {
    if (!sourceUtf8 || size < 4 || (size % 4) != 0) {
        return Result<Shader>::fail("SPIR-V blob empty or unaligned");
    }
    const auto* words = reinterpret_cast<const uint32_t*>(sourceUtf8);
    if (words[0] != 0x07230203u) {
        return Result<Shader>::fail("not a SPIR-V module");
    }
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = static_cast<size_t>(size);
    ci.pCode = words;
    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(impl_->device, &ci, nullptr, &mod) != VK_SUCCESS) {
        return Result<Shader>::fail("vkCreateShaderModule failed");
    }
    Shader s;
    s.handle_ = impl_->next++;
    if (s.handle_ >= impl_->shaders.size()) {
        impl_->shaders.resize(s.handle_ + 1);
    }
    impl_->shaders[s.handle_].module = mod;
    s.native_ = mod;
    return Result<Shader>::ok(s);
}

Result<Pipeline> Device::createPipeline(const PipelineDesc& desc) {
    if (desc.vertexShader >= impl_->shaders.size() || desc.fragmentShader >= impl_->shaders.size() ||
        !impl_->shaders[desc.vertexShader].module || !impl_->shaders[desc.fragmentShader].module) {
        return Result<Pipeline>::fail("invalid shader handles");
    }
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = impl_->shaders[desc.vertexShader].module;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = impl_->shaders[desc.fragmentShader].module;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState att{};
    att.blendEnable = VK_TRUE;
    att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstColorBlendFactor = dstFactor(desc.blend);
    att.colorBlendOp = VK_BLEND_OP_ADD;
    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstAlphaBlendFactor = dstFactor(desc.blend);
    att.alphaBlendOp = VK_BLEND_OP_ADD;
    att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &att;
    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    VkGraphicsPipelineCreateInfo gi{};
    gi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gi.stageCount = 2;
    gi.pStages = stages;
    gi.pVertexInputState = &vi;
    gi.pInputAssemblyState = &ia;
    gi.pViewportState = &vp;
    gi.pRasterizationState = &rs;
    gi.pMultisampleState = &ms;
    gi.pColorBlendState = &cb;
    gi.pDynamicState = &dyn;
    gi.layout = impl_->pipelineLayout;
    gi.renderPass = impl_->rpLoad;
    gi.subpass = 0;
    VkPipeline pipe = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(impl_->device, VK_NULL_HANDLE, 1, &gi, nullptr, &pipe) != VK_SUCCESS) {
        return Result<Pipeline>::fail("vkCreateGraphicsPipelines failed");
    }
    Pipeline p;
    p.handle_ = impl_->next++;
    if (p.handle_ >= impl_->pipelines.size()) {
        impl_->pipelines.resize(p.handle_ + 1);
    }
    impl_->pipelines[p.handle_].pipeline = pipe;
    p.native_ = pipe;
    return Result<Pipeline>::ok(p);
}

Result<Bindings> Device::createBindings(const BindingsDesc&) {
    Bindings b;
    b.handle_ = impl_->next++;
    return Result<Bindings>::ok(b);
}

CommandEncoder Device::encoder() {
    CommandEncoder enc;
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

void* Device::nativeDevice() const {
    return impl_ ? impl_->device : nullptr;
}

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

void* Device::nativeSampler() const {
    return impl_ ? impl_->sampler : nullptr;
}

}  // namespace glim::gpu
