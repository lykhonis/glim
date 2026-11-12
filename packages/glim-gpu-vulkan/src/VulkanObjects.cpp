#include "VulkanImpl.h"

namespace glim::gpu {

using namespace vulkan;

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
    if (impl_->makeImage(
            w,
            h,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            img,
            true
        ) != VK_SUCCESS) {
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
  if (impl_->makeBuffer(
          desc.size,
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
              VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
          buf
      ) != VK_SUCCESS) {
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

Result<Texture> Device::createTexture(const TextureDesc& desc) {
  const int w = std::max(desc.width, 1);
  const int h = std::max(desc.height, 1);
  GpuImage img{};
  if (impl_->makeImage(
          w,
          h,
          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
          img,
          true,
          VK_FORMAT_R8G8B8A8_UNORM
      ) != VK_SUCCESS) {
    return Result<Texture>::fail("Vulkan sampled image failed");
  }
  Texture t;
  t.handle_ = impl_->next++;
  t.width_ = w;
  t.height_ = h;
  if (t.handle_ >= impl_->sampled.size()) {
    impl_->sampled.resize(t.handle_ + 1);
  }
  impl_->sampled[t.handle_] = img;
  t.native_ = impl_->sampled[t.handle_].view;
  return Result<Texture>::ok(t);
}

void Device::writeTexture(Texture& texture, const void* rgba8, std::uint64_t bytes) {
  if (!impl_ || !rgba8 || texture.handle_ >= impl_->sampled.size()) {
    return;
  }
  GpuImage& img = impl_->sampled[texture.handle_];
  if (!img.image || texture.width() <= 0 || texture.height() <= 0) {
    return;
  }
  const std::uint64_t need = static_cast<std::uint64_t>(texture.width()) *
                             static_cast<std::uint64_t>(texture.height()) * 4ull;
  if (bytes < need) {
    return;
  }
  GpuBuffer staging{};
  if (impl_->makeBuffer(
          need,
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
          staging
      ) != VK_SUCCESS ||
      !staging.mapped) {
    impl_->destroyBuffer(staging);
    return;
  }
  std::memcpy(staging.mapped, rgba8, static_cast<std::size_t>(need));

  VkCommandBufferAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool = impl_->uploadPool ? impl_->uploadPool : impl_->cmdPool;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(impl_->device, &ai, &cmd) != VK_SUCCESS) {
    impl_->destroyBuffer(staging);
    return;
  }
  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &bi);
  imageBarrier(
      cmd,
      img.image,
      img.layout,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      0,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT
  );
  VkBufferImageCopy copy{};
  copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.imageSubresource.layerCount = 1;
  copy.imageExtent = {
      static_cast<uint32_t>(texture.width()), static_cast<uint32_t>(texture.height()), 1
  };
  vkCmdCopyBufferToImage(
      cmd, staging.buffer, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy
  );
  imageBarrier(
      cmd,
      img.image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
  );
  vkEndCommandBuffer(cmd);
  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cmd;
  if (vkQueueSubmit(impl_->queue, 1, &si, VK_NULL_HANDLE) != VK_SUCCESS) {
    vkFreeCommandBuffers(impl_->device, ai.commandPool, 1, &cmd);
    impl_->destroyBuffer(staging);
    return;
  }
  vkQueueWaitIdle(impl_->queue);
  vkFreeCommandBuffers(impl_->device, ai.commandPool, 1, &cmd);
  impl_->destroyBuffer(staging);
  img.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

Result<Texture> Device::wrapNativeTexture(void* native, int width, int height) {
  if (!impl_ || !native || width <= 0 || height <= 0) {
    return Result<Texture>::fail("wrapNativeTexture: null or empty");
  }
  GpuImage img{};
  img.view = reinterpret_cast<VkImageView>(native);
  img.width = width;
  img.height = height;
  img.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  img.borrowed = true;
  Texture t;
  t.handle_ = impl_->next++;
  t.width_ = width;
  t.height_ = height;
  if (t.handle_ >= impl_->sampled.size()) {
    impl_->sampled.resize(t.handle_ + 1);
  }
  impl_->sampled[t.handle_] = img;
  t.native_ = impl_->sampled[t.handle_].view;
  return Result<Texture>::ok(t);
}

Result<Texture> Device::importSurface(void* surface, int width, int height, SampleFormat format) {
#if !defined(VK_USE_PLATFORM_ANDROID_KHR)
  (void)surface;
  (void)width;
  (void)height;
  (void)format;
  return Result<Texture>::fail("importSurface: wrap a VkImageView");
#else
  if (!impl_ || !surface || width <= 0 || height <= 0) {
    return Result<Texture>::fail("importSurface: null or empty");
  }
  if (!impl_->haveAhb) {
    return Result<Texture>::fail("VK_ANDROID_external_memory_android_hardware_buffer missing");
  }
  auto* ahb = static_cast<AHardwareBuffer*>(surface);
  VkAndroidHardwareBufferFormatPropertiesANDROID fmtProps{};
  fmtProps.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;
  VkAndroidHardwareBufferPropertiesANDROID props{};
  props.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
  props.pNext = &fmtProps;
  if (vkGetAndroidHardwareBufferPropertiesANDROID(impl_->device, ahb, &props) != VK_SUCCESS) {
    return Result<Texture>::fail("vkGetAndroidHardwareBufferPropertiesANDROID failed");
  }
  if (fmtProps.format == VK_FORMAT_UNDEFINED) {
    return Result<Texture>::fail("YUV / external-format AHardwareBuffer is not in v1");
  }

  VkExternalMemoryImageCreateInfo extImg{};
  extImg.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  extImg.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

  VkImageCreateInfo ici{};
  ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ici.pNext = &extImg;
  ici.imageType = VK_IMAGE_TYPE_2D;
  ici.format = fmtProps.format;
  ici.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
  ici.mipLevels = 1;
  ici.arrayLayers = 1;
  ici.samples = VK_SAMPLE_COUNT_1_BIT;
  ici.tiling = VK_IMAGE_TILING_OPTIMAL;
  ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
  ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  GpuImage img{};
  img.width = width;
  img.height = height;
  if (vkCreateImage(impl_->device, &ici, nullptr, &img.image) != VK_SUCCESS) {
    return Result<Texture>::fail("AHB vkCreateImage failed");
  }

  VkImportAndroidHardwareBufferInfoANDROID importInfo{};
  importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
  importInfo.buffer = ahb;

  VkMemoryDedicatedAllocateInfo dedicated{};
  dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  dedicated.pNext = &importInfo;
  dedicated.image = img.image;

  VkMemoryAllocateInfo mai{};
  mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mai.pNext = &dedicated;
  mai.allocationSize = props.allocationSize;
  mai.memoryTypeIndex = findMemoryType(impl_->physical, props.memoryTypeBits, 0);
  if (mai.memoryTypeIndex == UINT32_MAX ||
      vkAllocateMemory(impl_->device, &mai, nullptr, &img.memory) != VK_SUCCESS) {
    impl_->destroyImage(img);
    return Result<Texture>::fail("AHB vkAllocateMemory failed");
  }
  if (vkBindImageMemory(impl_->device, img.image, img.memory, 0) != VK_SUCCESS) {
    impl_->destroyImage(img);
    return Result<Texture>::fail("AHB vkBindImageMemory failed");
  }

  VkImageViewCreateInfo vci{};
  vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  vci.image = img.image;
  vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vci.format = ici.format;
  vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vci.subresourceRange.levelCount = 1;
  vci.subresourceRange.layerCount = 1;
  if (format == SampleFormat::Bgra8Unorm && ici.format == VK_FORMAT_B8G8R8A8_UNORM) {
    vci.components.r = VK_COMPONENT_SWIZZLE_B;
    vci.components.b = VK_COMPONENT_SWIZZLE_R;
  }
  if (vkCreateImageView(impl_->device, &vci, nullptr, &img.view) != VK_SUCCESS) {
    impl_->destroyImage(img);
    return Result<Texture>::fail("AHB vkCreateImageView failed");
  }
  VkCommandPool pool = impl_->uploadPool ? impl_->uploadPool : impl_->cmdPool;
  if (pool) {
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(impl_->device, &ai, &cmd) == VK_SUCCESS) {
      VkCommandBufferBeginInfo bi{};
      bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      vkBeginCommandBuffer(cmd, &bi);
      imageBarrier(
          cmd,
          img.image,
          VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          0,
          VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
      );
      vkEndCommandBuffer(cmd);
      VkSubmitInfo si{};
      si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      si.commandBufferCount = 1;
      si.pCommandBuffers = &cmd;
      vkQueueSubmit(impl_->queue, 1, &si, VK_NULL_HANDLE);
      vkQueueWaitIdle(impl_->queue);
      vkFreeCommandBuffers(impl_->device, pool, 1, &cmd);
    }
  }
  img.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  Texture t;
  t.handle_ = impl_->next++;
  t.width_ = width;
  t.height_ = height;
  if (t.handle_ >= impl_->sampled.size()) {
    impl_->sampled.resize(t.handle_ + 1);
  }
  impl_->sampled[t.handle_] = img;
  t.native_ = impl_->sampled[t.handle_].view;
  return Result<Texture>::ok(t);
#endif
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

std::vector<Result<Shader>> Device::createShaders(std::span<const ShaderSource> sources) {
  return createShadersInOrder(*this, sources);
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
  if (vkCreateGraphicsPipelines(impl_->device, VK_NULL_HANDLE, 1, &gi, nullptr, &pipe) !=
      VK_SUCCESS) {
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

}  // namespace glim::gpu
