#include <glim/gpu/Device.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace glim::gpu {

Queue::Queue() = default;
Queue::Queue(void* native) : native_(native) {}

struct Pass::Impl {
  bool open = false;
};

Pass::Pass() : impl_(std::make_unique<Impl>()) {}
Pass::~Pass() = default;
Pass::Pass(Pass&&) noexcept = default;
Pass& Pass::operator=(Pass&&) noexcept = default;

void Pass::setPipeline(const Pipeline&) {}
void Pass::setBindings(std::uint32_t, const Bindings&) {}
void Pass::setVertexBuffer(std::uint32_t, const Buffer&, std::uint64_t) {}
void Pass::setBytes(std::uint32_t, const void*, std::uint64_t) {}
void Pass::setFragmentBytes(std::uint32_t, const void*, std::uint64_t) {}
void Pass::setFragmentTexture(std::uint32_t, void*) {}
void Pass::setFragmentSampler(std::uint32_t, void*) {}
void Pass::setViewport(float, float, float, float, float, float) {}
void Pass::setScissor(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {}
void Pass::draw(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {}
void Pass::drawIndexed(std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t) {}
void Pass::end() {}

struct CommandEncoder::Impl {
  bool open = false;
};

CommandEncoder::CommandEncoder() : impl_(std::make_unique<Impl>()) {}
CommandEncoder::~CommandEncoder() = default;
CommandEncoder::CommandEncoder(CommandEncoder&&) noexcept = default;
CommandEncoder& CommandEncoder::operator=(CommandEncoder&&) noexcept = default;

Pass CommandEncoder::beginPass(const PassDesc&) { return Pass(); }
bool CommandEncoder::copyColorTo(const FrameTarget&) { return false; }
void CommandEncoder::generateMips(const FrameTarget&) {}
void CommandEncoder::present(const Drawable&) {}
void CommandEncoder::submit(Queue&) {}

struct Device::Impl {
  DeviceCreateInfo info{};
  Queue queue{};
  int drawableW = 1;
  int drawableH = 1;
};

Result<Device> Device::create(const DeviceCreateInfo& info) {
  Device device;
  device.impl_ = std::unique_ptr<Impl>(new Impl());
  device.impl_->info = info;
  if (info.backend != Backend::WebGpu) {
    return Result<Device>::fail("glim-gpu-webgpu: DeviceCreateInfo.backend must be WebGpu");
  }
  return Result<Device>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}

Device::Device() = default;
Device::~Device() = default;
Device::Device(Device&&) noexcept = default;
Device& Device::operator=(Device&&) noexcept = default;

Queue& Device::queue() { return impl_->queue; }
Result<Drawable> Device::nextDrawable() {
  return Result<Drawable>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
void Device::setDrawableSize(int width, int height) {
  if (!impl_) {
    return;
  }
  if (width > 0) {
    impl_->drawableW = width;
  }
  if (height > 0) {
    impl_->drawableH = height;
  }
}
Extent Device::drawableSize() const {
  if (!impl_) {
    return {};
  }
  return {impl_->drawableW, impl_->drawableH};
}

int Device::presentRotationDegrees() const { return 0; }

float Device::lastGpuMs() const { return 0.f; }

void Device::notifyNextPresented(std::function<void()>) {}
Result<FrameTarget> Device::createFrameTarget(const FrameTargetDesc&) {
  return Result<FrameTarget>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}

bool Device::headless() const { return false; }

bool Device::readPixels(const FrameTarget&, void*, std::uint64_t) { return false; }
Result<Buffer> Device::createBuffer(const BufferDesc&) {
  return Result<Buffer>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
void Device::writeBuffer(Buffer&, const void*, std::uint64_t) {}
Result<Texture> Device::createTexture(const TextureDesc&) {
  return Result<Texture>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
void Device::writeTexture(Texture&, const void*, std::uint64_t) {}
Result<Texture> Device::wrapNativeTexture(void*, int, int) {
  return Result<Texture>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
Result<Texture> Device::importSurface(void*, int, int, SampleFormat) {
  return Result<Texture>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
Result<Shader> Device::createShader(ShaderStage, const char*, std::uint64_t) {
  return Result<Shader>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
std::vector<Result<Shader>> Device::createShaders(std::span<const ShaderSource> sources) {
  return createShadersInOrder(*this, sources);
}

void Device::prepareShaders(std::span<const ShaderSource>) {}
Result<Pipeline> Device::createPipeline(const PipelineDesc&) {
  return Result<Pipeline>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
Result<Bindings> Device::createBindings(const BindingsDesc&) {
  return Result<Bindings>::fail("glim-gpu-webgpu: browser target requires Emscripten");
}
CommandEncoder Device::encoder() { return CommandEncoder(); }

void* Device::nativeDevice() const { return nullptr; }
void* Device::nativeLayer() const { return nullptr; }
void* Device::nativeSampler() const { return nullptr; }
void* Device::colorNative() const { return nullptr; }

}  // namespace glim::gpu
