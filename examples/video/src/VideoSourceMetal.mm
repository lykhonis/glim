#import <IOSurface/IOSurfaceRef.h>
#import <Metal/Metal.h>

#include <array>
#include <atomic>
#include <string>

#include "ClipScene.h"
#include "VideoClip.h"
#include "VideoSource.h"

namespace glim {
namespace toolkit {

#if GLIM_SOFTWARE
// The CPU renderer cannot sample an IOSurface.
std::unique_ptr<VideoSource> makeVideoSource() { return nullptr; }
#else
namespace {

constexpr const char* kMetalPrelude = R"(
#include <metal_stdlib>
using namespace metal;
#define CONST constant
)";

constexpr const char* kEntry = R"(
kernel void frame(texture2d<half, access::write> out [[texture(0)]],
                  constant float& t [[buffer(0)]],
                  uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= out.get_width() || gid.y >= out.get_height()) return;
  float2 uv = float2(gid) / float2(out.get_width(), out.get_height());
  out.write(half4(half3(shade(uv, t, float2(gid))), 1.0h), gid);
}
)";

IOSurfaceRef makeSurface() {
  NSDictionary* props = @{
    (id)kIOSurfaceWidth : @(kClipWidth),
    (id)kIOSurfaceHeight : @(kClipHeight),
    (id)kIOSurfaceBytesPerElement : @4,
    (id)kIOSurfacePixelFormat : @(0x42475241),  // 'BGRA'
  };
  return IOSurfaceCreate((__bridge CFDictionaryRef)props);
}

class MetalVideoSource final : public VideoSource {
 public:
  MetalVideoSource(id<MTLDevice> device, id<MTLComputePipelineState> pipe)
      : pipe_(pipe),
        queue_([device newCommandQueue]),
        event_([device newSharedEvent]),
        feed_([event = event_] { return event.signaledValue; }) {
    MTLTextureDescriptor* desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:kClipWidth
                                                        height:kClipHeight
                                                     mipmapped:NO];
    desc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;
    for (std::size_t i = 0; i < kClipRing; ++i) {
      surfaces_[i] = makeSurface();
      textures_[i] = [device newTextureWithDescriptor:desc iosurface:surfaces_[i] plane:0];
    }
  }

  ~MetalVideoSource() override {
    if (timer_) {
      dispatch_source_cancel(timer_);
      dispatch_sync(serial_, ^{
                    });
    }
    id<MTLCommandBuffer> drain = [queue_ commandBuffer];
    [drain commit];
    [drain waitUntilCompleted];
    for (IOSurfaceRef s : surfaces_) CFRelease(s);
  }

  void attach(paint::Context& ctx) override {
    if (owner_ != nullptr) return;
    owner_ = &ctx;
    for (std::size_t i = 0; i < kClipRing; ++i) {
      ids_[i] = ctx.wrapSurface(surfaces_[i], kClipWidth, kClipHeight);
    }
    timer_ = dispatch_source_create(
DISPATCH_SOURCE_TYPE_TIMER, 0, 0, serial_);
    const auto interval = static_cast<std::uint64_t>(NSEC_PER_SEC / kClipFps);
    dispatch_source_set_timer(timer_, dispatch_time(DISPATCH_TIME_NOW, 0), interval, interval / 10);
    dispatch_source_set_event_handler(timer_, ^{
      produce();
    });
    dispatch_resume(timer_);
  }

  void setPlaying(bool playing) override { playing_.store(playing, std::memory_order_relaxed); }

  std::uint32_t acquire() override { return feed_.acquire(); }

  host::FeedStats stats() const override { return feed_.stats(); }

 private:
  void produce() {
    if (!playing_.load(std::memory_order_relaxed)) return;
    const std::optional<std::size_t> claimed = feed_.claim(ids_);
    if (!claimed) return;
    const std::size_t slot = *claimed;
    const float t = static_cast<float>(static_cast<double>(fence_) / kClipFps);
    id<MTLCommandBuffer> cb = [queue_ commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
    [enc setComputePipelineState:pipe_];
    [enc setTexture:textures_[slot] atIndex:0];
    [enc setBytes:&t length:sizeof t atIndex:0];
    [enc dispatchThreads:MTLSizeMake(kClipWidth, kClipHeight, 1)
      threadsPerThreadgroup:MTLSizeMake(16, 16, 1)];
    [enc endEncoding];
    ++fence_;
    [cb encodeSignalEvent:event_ value:fence_];
    [cb commit];
    feed_.publish({ids_[slot], fence_});
  }

  dispatch_queue_t serial_ = dispatch_queue_create("glim.video", DISPATCH_QUEUE_SERIAL);
  id<MTLComputePipelineState> pipe_;
  id<MTLCommandQueue> queue_;
  id<MTLSharedEvent> event_;
  host::Feed feed_;
  std::array<IOSurfaceRef, kClipRing> surfaces_{};
  std::array<id<MTLTexture>, kClipRing> textures_{};
  std::array<std::uint32_t, kClipRing> ids_{};
  paint::Context* owner_ = nullptr;
  dispatch_source_t timer_ = nullptr;
  std::atomic<bool> playing_{true};
  std::uint64_t fence_ = 0;
};

}  // namespace

std::unique_ptr<VideoSource> makeVideoSource() {
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (device == nil) return nullptr;
  NSError* error = nil;
  id<MTLLibrary> lib = [device newLibraryWithSource:@((std::string(kMetalPrelude) + kClipScene + kEntry).c_str()) options:nil error:&error];
  id<MTLFunction> fn = [lib newFunctionWithName:@"frame"];
  id<MTLComputePipelineState> pipe =
    fn ? [device newComputePipelineStateWithFunction:fn error:&error] : nil;
  if (pipe == nil) return nullptr;
  return std::make_unique<MetalVideoSource>(device, pipe);
}
#endif

}  // namespace toolkit
}  // namespace glim
