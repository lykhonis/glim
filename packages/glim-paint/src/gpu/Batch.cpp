#include <glim/paint/Renderer.h>

#include <algorithm>
#include <cstddef>

namespace glim::paint {

void* Renderer::TextureCache::get(std::uint32_t imageId) {
    if (!images_ || imageId == 0) {
        return nullptr;
    }
    if (imageId >= textures_.size()) {
        textures_.resize(imageId + 1);
    }
    gpu::Texture& tex = textures_[imageId];
    if (tex.native()) {
        return tex.native();
    }
    const StoredImage* img = images_->get(imageId);
    if (!img) {
        return nullptr;
    }
    if (img->native) {
        return img->native;
    }
    if (img->rgba.empty()) {
        return nullptr;
    }
    auto created = device_.createTexture({img->width, img->height});
    if (!created.ok()) {
        return nullptr;
    }
    tex = std::move(created.value());
    device_.writeTexture(tex, img->rgba.data(), img->rgba.size());
    return tex.native();
}

void Renderer::Batch::clearAll() {
    solid.clear();
    rounded.clear();
    gradient.clear();
    blit.clear();
    blitTex = nullptr;
    glyph.clear();
    glyphTex = nullptr;
}

void Renderer::Batch::flushSolid(gpu::Pass& pass, const Pipelines& pipes, Stats& stats) {
    if (solid.empty()) {
        return;
    }
    pass.setPipeline(pipes.solid);
    // 4 KB chunks: setBytes-style uploads stay within the fast path on both
    // backends (larger chunks risk heap-alloc fallback on Metal and dynamic
    // UBO alignment trouble on Vulkan). Raise only with device validation.
    constexpr std::size_t kMax = 4096 / sizeof(SolidInstance);
    std::size_t i = 0;
    while (i < solid.size()) {
        const std::size_t n = std::min(kMax, solid.size() - i);
        pass.setBytes(0, solid.data() + i, sizeof(SolidInstance) * n);
        pass.draw(6, static_cast<std::uint32_t>(n), 0, 0);
        stats.draws += 1;
        stats.instances += static_cast<unsigned>(n);
        i += n;
    }
    solid.clear();
}

void Renderer::Batch::flushRounded(gpu::Pass& pass, const Pipelines& pipes, Stats& stats) {
    if (rounded.empty()) {
        return;
    }
    if (!pipes.rounded.native()) {
        rounded.clear();
        return;
    }
    pass.setPipeline(pipes.rounded);
    constexpr std::size_t kMax = 4096 / sizeof(RoundedInstance);
    std::size_t i = 0;
    while (i < rounded.size()) {
        const std::size_t n = std::min(kMax, rounded.size() - i);
        const std::uint64_t bytes = sizeof(RoundedInstance) * n;
        pass.setBytes(0, rounded.data() + i, bytes);
        pass.setFragmentBytes(0, rounded.data() + i, bytes);
        pass.draw(6, static_cast<std::uint32_t>(n), 0, 0);
        stats.draws += 1;
        stats.instances += static_cast<unsigned>(n);
        i += n;
    }
    rounded.clear();
}

void Renderer::Batch::flushGradient(gpu::Pass& pass, const Pipelines& pipes, Stats& stats) {
    if (gradient.empty()) {
        return;
    }
    if (!pipes.gradient.native()) {
        gradient.clear();
        return;
    }
    pass.setPipeline(pipes.gradient);
    constexpr std::size_t kMax = 4096 / sizeof(GradientInstance);
    std::size_t i = 0;
    while (i < gradient.size()) {
        const std::size_t n = std::min(kMax, gradient.size() - i);
        const std::uint64_t bytes = sizeof(GradientInstance) * n;
        pass.setBytes(0, gradient.data() + i, bytes);
        pass.setFragmentBytes(0, gradient.data() + i, bytes);
        pass.draw(6, static_cast<std::uint32_t>(n), 0, 0);
        stats.draws += 1;
        stats.instances += static_cast<unsigned>(n);
        i += n;
    }
    gradient.clear();
}

void Renderer::Batch::flushBlit(gpu::Pass& pass, const Pipelines& pipes, void* sampler,
                                Stats& stats) {
    if (blit.empty() || !blitTex) {
        blit.clear();
        blitTex = nullptr;
        return;
    }
    pass.setPipeline(pipes.blit);
    pass.setFragmentTexture(0, blitTex);
    pass.setFragmentSampler(0, sampler);
    constexpr std::size_t kMax = 4096 / sizeof(BlitInstance);
    std::size_t i = 0;
    while (i < blit.size()) {
        const std::size_t n = std::min(kMax, blit.size() - i);
        pass.setBytes(0, blit.data() + i, sizeof(BlitInstance) * n);
        pass.draw(6, static_cast<std::uint32_t>(n), 0, 0);
        stats.draws += 1;
        stats.instances += static_cast<unsigned>(n);
        i += n;
    }
    blit.clear();
    blitTex = nullptr;
}

void Renderer::Batch::flushGlyph(gpu::Pass& pass, const Pipelines& pipes, void* sampler,
                                 Stats& stats) {
    if (glyph.empty() || !glyphTex) {
        glyph.clear();
        glyphTex = nullptr;
        return;
    }
    pass.setPipeline(pipes.glyph);
    pass.setFragmentTexture(0, glyphTex);
    pass.setFragmentSampler(0, sampler);
    constexpr std::size_t kMax = 4096 / sizeof(BlitInstance);
    std::size_t i = 0;
    while (i < glyph.size()) {
        const std::size_t n = std::min(kMax, glyph.size() - i);
        pass.setBytes(0, glyph.data() + i, sizeof(BlitInstance) * n);
        pass.draw(6, static_cast<std::uint32_t>(n), 0, 0);
        stats.draws += 1;
        stats.instances += static_cast<unsigned>(n);
        i += n;
    }
    glyph.clear();
    glyphTex = nullptr;
}

}  // namespace glim::paint
