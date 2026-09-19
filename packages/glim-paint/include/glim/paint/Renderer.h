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
    void setTarget(std::uint8_t* rgba, int width, int height);
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

#if !GLIM_SOFTWARE
public:
    struct SolidInstance {
        float rect[4];
        float color[4];
    };
    struct RoundedInstance {
        float rect[4];
        float radii[4];
        float color[4];
        float extra[4];
    };
    struct BlitInstance {
        float rect[4];
        float uv[4];
        float extra[4];
    };
    struct GradientInstance {
        float rect[4];
        float grad[4];
        float misc[4];
        float colors[32];
        float offsets[8];
    };
    struct Pipelines {
        bool ensure(gpu::Device& device);
        gpu::Pipeline solid{};
        gpu::Pipeline rounded{};
        gpu::Pipeline blit{};
        gpu::Pipeline glyph{};
        gpu::Pipeline blur{};
        gpu::Pipeline blur1d{};
        gpu::Pipeline glass{};
        gpu::Pipeline gradient{};
        bool ready_ = false;
    };
    struct TextureCache {
        explicit TextureCache(gpu::Device& device) : device_(device) {}
        void setImages(const ImageStore* images) { images_ = images; }
        void* get(std::uint32_t imageId);
        gpu::Device& device_;
        const ImageStore* images_ = nullptr;
        std::vector<gpu::Texture> textures_;
    };
    struct Batch {
        void clearAll();
        void flushSolid(gpu::Pass& pass, const Pipelines& pipes, Stats& stats);
        void flushRounded(gpu::Pass& pass, const Pipelines& pipes, Stats& stats);
        void flushGradient(gpu::Pass& pass, const Pipelines& pipes, Stats& stats);
        void flushBlit(gpu::Pass& pass, const Pipelines& pipes, void* sampler, Stats& stats);
        void flushGlyph(gpu::Pass& pass, const Pipelines& pipes, void* sampler, Stats& stats);
        std::vector<SolidInstance> solid;
        std::vector<RoundedInstance> rounded;
        std::vector<GradientInstance> gradient;
        std::vector<BlitInstance> blit;
        void* blitTex = nullptr;
        std::vector<BlitInstance> glyph;
        void* glyphTex = nullptr;
    };
    struct TargetPool {
        gpu::FrameTarget* acquire(gpu::Device& device, int w, int h);
        void reset() { used_ = 0; }
        gpu::FrameTarget backdrop;
        gpu::FrameTarget glassSrc;
        gpu::FrameTarget glassBlurTmp;
        gpu::FrameTarget glassBlur;
        struct Slot {
            gpu::FrameTarget ft;
            int w = 0;
            int h = 0;
        };
        std::vector<Slot> slots_;
        int used_ = 0;
    };
private:
#if GLIM_EMBED
    void submitLayer(gpu::CommandEncoder& encoder, const std::vector<Quad>& quads,
                     const std::vector<BlitQuad>& blits, const std::vector<GradientQuad>& gradients,
                     const std::vector<Isolate>& isolates, const Mat4& projection, int viewportW,
                     int viewportH, void* nativeColor, gpu::LoadOp load);
#else
    void encodeGroup(gpu::CommandEncoder& encoder, const Group& group, const Mat4& projection,
                     int viewportW, int viewportH, void* nativeColor, gpu::LoadOp load,
                     float pixelRatio, Vec2 logicalSize, const Mat4& extra = Mat4::identity());
#endif
    gpu::Device& device_;
    Pipelines pipes_;
    TargetPool targets_;
    TextureCache textures_;
    Batch batch_;
#else
    std::uint8_t* rgba_ = nullptr;
    int width_ = 0;
    int height_ = 0;
#endif
    Stats stats_{};
};

}  // namespace glim::paint
