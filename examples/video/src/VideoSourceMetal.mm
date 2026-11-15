#import <IOSurface/IOSurfaceRef.h>
#import <Metal/Metal.h>

#include <array>
#include <atomic>

#include "VideoSource.h"

namespace glim {
namespace toolkit {

#if GLIM_SOFTWARE
// The CPU renderer cannot sample an IOSurface.
std::unique_ptr<VideoSource> makeVideoSource() { return nullptr; }
#else
namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 360;
constexpr double kFps = 30.0;
// Three buffers: one shown, one in flight, one being written.
constexpr std::size_t kRing = 3;

// A dusk flyover over a lake, so motion, gradients and fine detail all
// change every frame the way decoded footage does.
constexpr const char* kKernel = R"(
#include <metal_stdlib>
using namespace metal;

float hash(float2 p) { return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }

float noise(float2 p) {
  float2 i = floor(p), f = fract(p);
  float2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash(i), hash(i + float2(1, 0)), u.x),
             mix(hash(i + float2(0, 1)), hash(i + float2(1, 1)), u.x), u.y);
}

float fbm(float2 p, int octaves) {
  float v = 0.0, a = 0.5;
  for (int i = 0; i < octaves; ++i) { v += a * noise(p); p *= 2.03; a *= 0.5; }
  return v;
}

constant float kHorizon = 0.66;

float3 above(float2 uv, float t) {
  const float pan = t * 0.04;
  float3 c = mix(float3(0.98, 0.62, 0.36), float3(0.16, 0.24, 0.46), pow(1.0 - uv.y / kHorizon, 1.4));
  const float2 sun = float2(0.68, 0.40);
  const float d = length((uv - sun) * float2(1.78, 1.0));
  c += float3(1.0, 0.72, 0.42) * (0.55 * exp(-d * 7.0));
  c = mix(c, float3(1.0, 0.95, 0.82), smoothstep(0.05, 0.043, d));
  const float cloud = fbm(float2(uv.x * 3.0 + pan * 0.6 + t * 0.015, uv.y * 9.0), 5);
  c = mix(c, mix(float3(0.95, 0.62, 0.50), float3(0.36, 0.30, 0.42), uv.y / kHorizon),
          smoothstep(0.5, 0.78, cloud) * smoothstep(kHorizon, 0.15, uv.y) * 0.8);
  for (int k = 0; k < 3; ++k) {
    const float depth = float(k);
    const float x = uv.x * (1.5 + depth) + pan * (0.4 + depth * 0.5) + depth * 7.0;
    const float ridge = kHorizon - (0.20 - depth * 0.05) * fbm(float2(x, depth * 3.1), 4 - k);
    const float3 tone = mix(float3(0.46, 0.34, 0.44), float3(0.07, 0.09, 0.10), depth / 2.0);
    c = mix(c, tone, smoothstep(ridge, ridge + 0.002, uv.y));
  }
  return c;
}

kernel void frame(texture2d<half, access::write> out [[texture(0)]],
                  constant float& t [[buffer(0)]],
                  uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= out.get_width() || gid.y >= out.get_height()) return;
  float2 uv = float2(gid) / float2(out.get_width(), out.get_height());
  float3 c;
  if (uv.y < kHorizon) {
    c = above(uv, t);
  } else {
    const float below = uv.y - kHorizon;
    const float ripple = (fbm(float2(uv.x * 6.0, below * 60.0 - t * 1.5), 3) - 0.5) * below * 0.25;
    c = above(float2(uv.x + ripple, kHorizon - below * 1.2), t) * float3(0.74, 0.66, 0.62);
    c += 0.08 * smoothstep(0.72, 0.9, noise(float2(uv.x * 90.0, below * 300.0 - t * 4.0)));
  }
  const float2 v = uv - 0.5;
  c *= 1.0 - dot(v, v) * 0.9;
  c += (hash(float2(gid) + fract(t) * 917.0) - 0.5) * 0.04;
  out.write(half4(half3(saturate(c)), 1.0h), gid);
}
)";

IOSurfaceRef makeSurface() {
  NSDictionary* props = @{
    (id)kIOSurfaceWidth : @(kWidth),
    (id)kIOSurfaceHeight : @(kHeight),
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
                                                         width:kWidth
                                                        height:kHeight
                                                     mipmapped:NO];
    desc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;
    for (std::size_t i = 0; i < kRing; ++i) {
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
    for (std::size_t i = 0; i < kRing; ++i) {
      ids_[i] = ctx.wrapSurface(surfaces_[i], kWidth, kHeight);
    }
    timer_ = dispatch_source_create(
DISPATCH_SOURCE_TYPE_TIMER, 0, 0, serial_);
    const auto interval = static_cast<std::uint64_t>(NSEC_PER_SEC / kFps);
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
    const std::size_t slot = fence_ % kRing;
    const float t = static_cast<float>(static_cast<double>(fence_) / kFps);
    id<MTLCommandBuffer> cb = [queue_ commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
    [enc setComputePipelineState:pipe_];
    [enc setTexture:textures_[slot] atIndex:0];
    [enc setBytes:&t length:sizeof t atIndex:0];
    [enc dispatchThreads:MTLSizeMake(kWidth, kHeight, 1)
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
  std::array<IOSurfaceRef, kRing> surfaces_{};
  std::array<id<MTLTexture>, kRing> textures_{};
  std::array<std::uint32_t, kRing> ids_{};
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
  id<MTLLibrary> lib = [device newLibraryWithSource:@(kKernel) options:nil error:&error];
  id<MTLFunction> fn = [lib newFunctionWithName:@"frame"];
  id<MTLComputePipelineState> pipe =
    fn ? [device newComputePipelineStateWithFunction:fn error:&error] : nil;
  if (pipe == nil) return nullptr;
  return std::make_unique<MetalVideoSource>(device, pipe);
}
#endif

}  // namespace toolkit
}  // namespace glim
