#include <glim/paint/Renderer.h>

#include <algorithm>
#include <chrono>

#include "Isolates.h"

namespace glim::paint {

void Renderer::draw(const Scene& scene) {
    const auto t0 = std::chrono::steady_clock::now();
    stats_ = Stats{};
    if (!pipes_.ensure(device_)) {
        return;
    }
    auto drawable = device_.nextDrawable();
    if (!drawable.ok()) {
        return;
    }
    targets_.reset();
    const int rotation = device_.presentRotationDegrees();
    const bool swapped = (rotation == 90 || rotation == 270);
    const float logicalW = swapped ? scene.logicalSize.y : scene.logicalSize.x;
    const float logicalH = swapped ? scene.logicalSize.x : scene.logicalSize.y;
    // Width and height ratios agree when the drawable aspect matches logical.
    // On mismatch (letterbox/stretch) take the larger so isolates never
    // undersample an axis.
    const float prW =
        logicalW > 0.f ? static_cast<float>(drawable->width()) / logicalW : 1.f;
    const float prH =
        logicalH > 0.f ? static_cast<float>(drawable->height()) / logicalH : 1.f;
    const float pr = std::max(prW > 0.f ? prW : 1.f, prH > 0.f ? prH : 1.f);
    FramePacket packet = reuse_.encode(scene, pr);
    stats_ = packet.stats;
    stats_.draws = 0;
    stats_.instances = 0;
    textures_.setImages(&packet.images);
    for (const BlitQuad& q : packet.blits) {
        textures_.get(q.imageId);
    }
    gpu::CommandEncoder encoder = device_.encoder();
    const Mat4 proj = presentProjection(Mat4::orthoYDown(0, 0, scene.logicalSize.x, scene.logicalSize.y),
                                        rotation);
    submitLayer(encoder, packet.quads, packet.blits, packet.gradients, packet.isolates, proj,
                drawable->width(), drawable->height(), nullptr, gpu::LoadOp::Clear);
    encoder.present(drawable.value());
    encoder.submit(device_.queue());
    stats_.encodeMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace glim::paint
