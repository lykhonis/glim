#include <glim/paint/Renderer.h>

#if GLIM_SOFTWARE
#include <glim/paint/Software.h>
#else
#include <glim/paint/Reuse.h>
#endif

namespace glim::paint {
#if !GLIM_SOFTWARE

Renderer::Renderer(gpu::Device& device) : device_(device), textures_(device) {}

Renderer::~Renderer() = default;

Stats Renderer::stats() const {
  Stats s = stats_;
  s.textureBytes = textures_.uploadedBytes;
  s.importedBytes = textures_.importedBytes;
  s.targetBytes = targets_.bytes();
  return s;
}

#else

Renderer::Renderer(std::uint8_t* rgba, int width, int height)
    : rgba_(rgba), width_(width), height_(height) {}

void Renderer::setTarget(std::uint8_t* rgba, int width, int height) {
  rgba_ = rgba;
  width_ = width;
  height_ = height;
}

Renderer::~Renderer() = default;

Stats Renderer::stats() const { return stats_; }

#if GLIM_EMBED

void Renderer::submit(const FramePacket& packet) {
  stats_ = packet.stats;
  if (!rgba_ || width_ <= 0 || height_ <= 0) {
    return;
  }
  rasterPacket(packet, width_, height_, rgba_);
}

#else

void Renderer::draw(const Scene& scene) {
  stats_ = Stats{};
  if (!rgba_ || width_ <= 0 || height_ <= 0) {
    return;
  }
  rasterScene(scene, width_, height_, rgba_);
}

#endif
#endif
}  // namespace glim::paint
