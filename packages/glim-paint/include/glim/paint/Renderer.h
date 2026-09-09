#pragma once

#include <glim/gpu/Device.h>
#include <glim/paint/Context.h>

namespace glim::paint {

class Renderer {
public:
    explicit Renderer(gpu::Device& device);
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    ~Renderer();

    void draw(const Scene& scene);

private:
    bool ensurePipeline();

    gpu::Device& device_;
    gpu::Pipeline pipeline_{};
    gpu::Buffer instanceBuffer_{};
    bool ready_ = false;
};

}  // namespace glim::paint
