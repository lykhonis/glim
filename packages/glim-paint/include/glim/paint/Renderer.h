#pragma once

#include <vector>

#include <glim/gpu/Device.h>
#include <glim/paint/Context.h>
#include <glim/paint/Scene.h>

namespace glim::paint {

class Renderer {
public:
    explicit Renderer(gpu::Device& device);
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    ~Renderer();

    void draw(const Scene& scene);
    const Stats& stats() const { return stats_; }

private:
    bool ensurePipelines();
    void encodeGroup(gpu::CommandEncoder& encoder, const Group& group, const Mat4& projection,
                     int viewportW, int viewportH, void* nativeColor, gpu::LoadOp load);
    void flushSolid(gpu::Pass& pass);

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
