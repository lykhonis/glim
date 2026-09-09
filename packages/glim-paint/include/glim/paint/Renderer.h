#pragma once

#include <vector>

#include <glim/gpu/Device.h>
#include <glim/paint/FramePacket.h>
#include <glim/paint/Scene.h>

namespace glim::paint {

// GPU submitter. Does not merge or record — that is encode(), which has no
// Device and can compile to wasm32. draw() is encode + submit for in-process use.
class Renderer {
public:
    explicit Renderer(gpu::Device& device);
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    ~Renderer();

    void draw(const Scene& scene);
    void submit(const FramePacket& packet);
    const Stats& stats() const { return stats_; }

private:
    bool ensurePipelines();
    void flushSolid(gpu::Pass& pass);
    void submitLayer(gpu::CommandEncoder& encoder, const std::vector<Quad>& quads,
                     const std::vector<Isolate>& isolates, const Mat4& projection, int viewportW,
                     int viewportH, void* nativeColor, gpu::LoadOp load);

    gpu::Device& device_;
    gpu::Pipeline solid_{};
    gpu::Pipeline blit_{};
    gpu::Buffer instanceBuffer_{};
    bool ready_ = false;
    Stats stats_{};

    struct SolidInstance {
        float rect[4];
        float color[4];
    };
    std::vector<SolidInstance> pending_;
};

}  // namespace glim::paint
