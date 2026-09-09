#pragma once

#ifndef GLIM_SOFTWARE
#define GLIM_SOFTWARE 0
#endif
#ifndef GLIM_EMBED
#define GLIM_EMBED 0
#endif

#if !GLIM_SOFTWARE
#include <glim/gpu/Device.h>
#endif
#if GLIM_EMBED
#include <glim/paint/FramePacket.h>
#else
#include <glim/paint/Scene.h>
#endif

#include <cstdint>
#include <vector>

namespace glim::paint {

class Renderer {
public:
#if GLIM_SOFTWARE
    Renderer(std::uint8_t* rgba, int width, int height);
#else
    explicit Renderer(gpu::Device& device);
#endif
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    ~Renderer();

#if GLIM_EMBED
    void submit(const FramePacket& packet);
#else
    void draw(const Scene& scene);
#endif
    const Stats& stats() const { return stats_; }

private:
#if !GLIM_SOFTWARE
    bool ensurePipelines();
    void flushSolid(gpu::Pass& pass);
#if GLIM_EMBED
    void submitLayer(gpu::CommandEncoder& encoder, const std::vector<Quad>& quads,
                     const std::vector<Isolate>& isolates, const Mat4& projection, int viewportW,
                     int viewportH, void* nativeColor, gpu::LoadOp load);
#else
    void encodeGroup(gpu::CommandEncoder& encoder, const Group& group, const Mat4& projection,
                     int viewportW, int viewportH, void* nativeColor, gpu::LoadOp load);
#endif
    gpu::Device& device_;
    gpu::Pipeline solid_{};
    gpu::Pipeline blit_{};
    gpu::Buffer instanceBuffer_{};
    bool ready_ = false;
    struct SolidInstance {
        float rect[4];
        float color[4];
    };
    std::vector<SolidInstance> pending_;
#else
    std::uint8_t* rgba_ = nullptr;
    int width_ = 0;
    int height_ = 0;
#endif
    Stats stats_{};
};

}  // namespace glim::paint
