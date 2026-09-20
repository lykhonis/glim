// WebGPU Device skeleton (PR 15 slice 1). Stub only; real implementation
// lands in slice 2. See docs/webgpu.md.

#include <glim/gpu/Device.h>

namespace glim::gpu {

namespace {
constexpr const char* kMsg =
    "glim-gpu-webgpu skeleton: full WebGPU implementation lands in PR 15 slice 2 "
    "(see docs/webgpu.md). Build with Emscripten for the browser target.";
}  // namespace

Queue::Queue() = default;
Queue::Queue(void* native) : native_(native) {}

struct Pass::Impl {
    bool open = false;
};

Pass::Pass() : impl_(new Impl()) {}
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
void Pass::end() {
    if (impl_) {
        impl_->open = false;
    }
}

struct CommandEncoder::Impl {
    bool open = false;
};

CommandEncoder::CommandEncoder() : impl_(new Impl()) {}
CommandEncoder::~CommandEncoder() = default;
CommandEncoder::CommandEncoder(CommandEncoder&&) noexcept = default;
CommandEncoder& CommandEncoder::operator=(CommandEncoder&&) noexcept = default;

Pass CommandEncoder::beginPass(const PassDesc&) {
    return Pass();
}
bool CommandEncoder::copyColorTo(const FrameTarget&) {
    return false;
}
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
#if defined(__EMSCRIPTEN__)
    return Result<Device>::fail(kMsg);
#else
    (void)kMsg;
    return Result<Device>::fail(
        "glim-gpu-webgpu: browser target requires Emscripten (see docs/webgpu.md)");
#endif
}

Device::Device() = default;
Device::~Device() = default;
Device::Device(Device&&) noexcept = default;
Device& Device::operator=(Device&&) noexcept = default;

Queue& Device::queue() {
    return impl_->queue;
}
Result<Drawable> Device::nextDrawable() {
    return Result<Drawable>::fail(kMsg);
}
void Device::setDrawableSize(int width, int height) {
    if (!impl_) {
        return;
    }
    impl_->drawableW = width;
    impl_->drawableH = height;
}
int Device::presentRotationDegrees() const {
    return 0;
}
Result<FrameTarget> Device::createFrameTarget(const FrameTargetDesc&) {
    return Result<FrameTarget>::fail(kMsg);
}
Result<Buffer> Device::createBuffer(const BufferDesc&) {
    return Result<Buffer>::fail(kMsg);
}
void Device::writeBuffer(Buffer&, const void*, std::uint64_t) {}
Result<Texture> Device::createTexture(const TextureDesc&) {
    return Result<Texture>::fail(kMsg);
}
void Device::writeTexture(Texture&, const void*, std::uint64_t) {}
Result<Texture> Device::wrapNativeTexture(void*, int, int) {
    return Result<Texture>::fail(kMsg);
}
Result<Texture> Device::importSurface(void*, int, int, SampleFormat) {
    return Result<Texture>::fail(kMsg);
}
Result<Shader> Device::createShader(ShaderStage, const char*, std::uint64_t) {
    return Result<Shader>::fail(kMsg);
}
Result<Pipeline> Device::createPipeline(const PipelineDesc&) {
    return Result<Pipeline>::fail(kMsg);
}
Result<Bindings> Device::createBindings(const BindingsDesc&) {
    return Result<Bindings>::fail(kMsg);
}
CommandEncoder Device::encoder() {
    return CommandEncoder();
}

void* Device::nativeDevice() const {
    return nullptr;
}
void* Device::nativeLayer() const {
    return nullptr;
}
void* Device::nativeSampler() const {
    return nullptr;
}
void* Device::colorNative() const {
    return nullptr;
}

}  // namespace glim::gpu
