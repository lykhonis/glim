#include "WebClip.h"

#include <webgpu/webgpu.h>

#include "ClipScene.h"

namespace {

using glim::toolkit::kClipHeight;
using glim::toolkit::kClipWidth;

extern "C" int glimClipDraw(
  void* device, void* texture, const char* scene, int w, int h, double t
);

}  // namespace

// One queue orders each copy before the frame that samples it, so a
// published buffer is ready at once.
WebClip::WebClip() : feed_([this] { return fence_; }) {}

WebClip::~WebClip() {
  for (void* t : textures_) {
    if (t) wgpuTextureRelease(static_cast<WGPUTexture>(t));
  }
}

bool WebClip::attach(
  glim::gpu::Device& device, glim::paint::Context& ctx
) {
  if (device_) return true;
  device_ = device.nativeDevice();
  WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
  desc.usage = WGPUTextureUsage_TextureBinding |
               WGPUTextureUsage_CopyDst |
               WGPUTextureUsage_RenderAttachment;
  desc.size = {kClipWidth, kClipHeight, 1};
  desc.format = WGPUTextureFormat_RGBA8Unorm;
  for (std::size_t i = 0; i < textures_.size(); ++i) {
    textures_[i] =
      wgpuDeviceCreateTexture(static_cast<WGPUDevice>(device_), &desc);
    ids_[i] = ctx.wrapSurface(
      textures_[i],
      kClipWidth,
      kClipHeight,
      glim::paint::SampleFormat::Rgba8Unorm
    );
  }
  return glimClipDraw(
           device_,
           textures_[0],
           glim::toolkit::kClipScene,
           kClipWidth,
           kClipHeight,
           0.0
         ) != 0;
}

void WebClip::produce(double timeSeconds) {
  if (!device_) return;
  const std::optional<std::size_t> slot = feed_.claim(ids_);
  if (!slot) return;
  if (!glimClipDraw(
        device_,
        textures_[*slot],
        glim::toolkit::kClipScene,
        kClipWidth,
        kClipHeight,
        timeSeconds
      )) {
    return;
  }
  ++fence_;
  feed_.publish({ids_[*slot], fence_});
}
