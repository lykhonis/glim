#include <glim/paint/Renderer.h>

#include <algorithm>
#include <chrono>

#include "Isolates.h"

namespace glim::paint {

void Renderer::submitLayer(gpu::CommandEncoder& encoder, const std::vector<Quad>& quads,
                           const std::vector<BlitQuad>& blits, const std::vector<GradientQuad>& gradients,
                           const std::vector<Isolate>& isolates, const Mat4& projection, int viewportW,
                           int viewportH, void* nativeColor, gpu::LoadOp load) {
    gpu::PassDesc passDesc;
    passDesc.nativeColor = nativeColor;
    passDesc.load = load;
    passDesc.clear = {0, 0, 0, nativeColor ? 0.f : 1.f};
    passDesc.viewportW = viewportW;
    passDesc.viewportH = viewportH;
    gpu::Pass pass = encoder.beginPass(passDesc);
    setProjection(pass, projection);

    batch_.clearAll();
    for (const Quad& q : quads) {
        SolidInstance inst{};
        inst.rect[0] = q.x;
        inst.rect[1] = q.y;
        inst.rect[2] = q.w;
        inst.rect[3] = q.h;
        inst.color[0] = q.r;
        inst.color[1] = q.g;
        inst.color[2] = q.b;
        inst.color[3] = q.a;
        batch_.solid.push_back(inst);
    }
    batch_.flushSolid(pass, pipes_, stats_);

    for (const GradientQuad& q : gradients) {
        if (q.w <= 0.f || q.h <= 0.f || q.coverage <= 1e-4f || q.stopCount == 0) {
            continue;
        }
        GradientInstance inst{};
        inst.rect[0] = q.x;
        inst.rect[1] = q.y;
        inst.rect[2] = q.w;
        inst.rect[3] = q.h;
        inst.grad[0] = q.p0x;
        inst.grad[1] = q.p0y;
        inst.grad[2] = q.p1x;
        inst.grad[3] = q.p1y;
        inst.misc[0] = static_cast<float>(q.kind);
        inst.misc[1] = q.radius;
        inst.misc[2] = q.coverage;
        inst.misc[3] = static_cast<float>(q.stopCount);
        for (int i = 0; i < q.stopCount && i < kMaxGradientStops; ++i) {
            inst.colors[i * 4 + 0] = q.r[i];
            inst.colors[i * 4 + 1] = q.g[i];
            inst.colors[i * 4 + 2] = q.b[i];
            inst.colors[i * 4 + 3] = q.a[i];
            inst.offsets[i] = q.offsets[i];
        }
        batch_.gradient.push_back(inst);
    }
    batch_.flushGradient(pass, pipes_, stats_);

    auto pushSampled = [this, &pass](const BlitQuad& q) {
        void* tex = textures_.get(q.imageId);
        if (!tex) {
            return;
        }
        BlitInstance inst{};
        inst.rect[0] = q.x;
        inst.rect[1] = q.y;
        inst.rect[2] = q.w;
        inst.rect[3] = q.h;
        inst.uv[0] = q.u0;
        inst.uv[1] = q.v0;
        inst.uv[2] = q.u1;
        inst.uv[3] = q.v1;
        inst.extra[0] = q.r;
        inst.extra[1] = q.g;
        inst.extra[2] = q.b;
        inst.extra[3] = q.a;
        if (q.sdf) {
            batch_.flushBlit(pass, pipes_, device_.nativeSampler(), stats_);
            if (batch_.glyphTex && batch_.glyphTex != tex) {
                batch_.flushGlyph(pass, pipes_, device_.nativeSampler(), stats_);
            }
            batch_.glyphTex = tex;
            batch_.glyph.push_back(inst);
            return;
        }
        batch_.flushGlyph(pass, pipes_, device_.nativeSampler(), stats_);
        if (batch_.blitTex && batch_.blitTex != tex) {
            batch_.flushBlit(pass, pipes_, device_.nativeSampler(), stats_);
        }
        batch_.blitTex = tex;
        batch_.blit.push_back(inst);
    };
    for (const BlitQuad& q : blits) {
        pushSampled(q);
    }
    batch_.flushBlit(pass, pipes_, device_.nativeSampler(), stats_);
    batch_.flushGlyph(pass, pipes_, device_.nativeSampler(), stats_);

    bool glassFrozen = false;
    bool snapshotted = false;
    for (const Isolate& iso : isolates) {
        gpu::FrameTarget* ft = targets_.acquire(device_, iso.contentW, iso.contentH);
        if (!ft || !ft->native()) {
            continue;
        }
        const float isoU1 =
            static_cast<float>(iso.contentW) / static_cast<float>(std::max(1, ft->width()));
        const float isoV1 =
            static_cast<float>(iso.contentH) / static_cast<float>(std::max(1, ft->height()));
        pass.end();
        gpu::LoadOp plateLoad = gpu::LoadOp::Clear;
        if (iso.hasGlass) {
            const int hw = std::max(1, viewportW / 2);
            const int hh = std::max(1, viewportH / 2);
            if (!glassFrozen) {
                if (!targets_.glassSrc.native() || targets_.glassSrc.width() != hw ||
                    targets_.glassSrc.height() != hh) {
                    auto bg = device_.createFrameTarget({hw, hh});
                    if (bg.ok()) {
                        targets_.glassSrc = std::move(bg.value());
                    }
                }
                void* src = nativeColor ? nativeColor : device_.colorNative();
                if (targets_.glassSrc.native() && src) {
                    blitTexture(encoder, pipes_, device_.nativeSampler(), stats_,
                                targets_.glassSrc.native(), hw, hh, src, 0.f, 0.f,
                                static_cast<float>(hw), static_cast<float>(hh), 0.f, 0.f, 1.f, 1.f,
                                1.f, 1.f, 1.f, 1.f);
                    glassFrozen = true;
                }
            }
            if (targets_.glassSrc.native()) {
                    const GlassUniforms gu = makeGlassUniforms(
                        iso.glass, iso.glassPills.empty() ? nullptr : iso.glassPills.data(),
                        static_cast<int>(iso.glassPills.size()), iso.contentW, iso.contentH,
                        iso.destW > 0.f ? static_cast<float>(iso.contentW) / iso.destW : 1.f,
                        iso.glassU0, iso.glassV0, iso.glassU1, iso.glassV1);
                    void* sharp = targets_.glassSrc.native();
                    void* blur = sharp;
                    const float blurR = glassBlurRadius(iso.glass);
                    if (blurR > 0.f && pipes_.blur1d.native() && !iso.glass.flatten) {
                        const float pr =
                            iso.destW > 0.f ? static_cast<float>(iso.contentW) / iso.destW : 1.f;
                        const float sigmaFull = blurR * pr * 0.5f;
                        blur = blurGlass(encoder, pipes_, device_.nativeSampler(), device_,
                                         targets_, sharp, iso.contentW, iso.contentH, iso.glassU0,
                                         iso.glassV0, iso.glassU1, iso.glassV1, sigmaFull, nullptr);
                    }
                    if (pipes_.glass.native() && !iso.glassPills.empty()) {
                        gpu::PassDesc plate;
                        plate.nativeColor = ft->native();
                        plate.load = gpu::LoadOp::Clear;
                        plate.clear = {0, 0, 0, 0};
                        plate.viewportW = iso.contentW;
                        plate.viewportH = iso.contentH;
                        gpu::Pass gp = encoder.beginPass(plate);
                        const Mat4 localProj = Mat4::orthoYDown(
                            0, 0, iso.destW > 0.f ? iso.destW : static_cast<float>(iso.contentW),
                            iso.destH > 0.f ? iso.destH : static_cast<float>(iso.contentH));
                        setProjection(gp, localProj);
                        gp.setPipeline(pipes_.glass);
                        gp.setBytes(0, &gu, sizeof(gu));
                        gp.setFragmentBytes(0, &gu, sizeof(gu));
                        gp.setFragmentTexture(0, sharp);
                        gp.setFragmentTexture(1, blur);
                        gp.setFragmentSampler(0, device_.nativeSampler());
                        gp.setFragmentSampler(1, device_.nativeSampler());
                        gp.draw(6, 1, 0, 0);
                        gp.end();
                    } else {
                        blitTexture(encoder, pipes_, device_.nativeSampler(), stats_, ft->native(),
                                    iso.contentW, iso.contentH, sharp, 0.f, 0.f,
                                    static_cast<float>(iso.contentW),
                                    static_cast<float>(iso.contentH), iso.glassU0, iso.glassV0,
                                    iso.glassU1, iso.glassV1, 1.f, 1.f, 1.f, 1.f);
                    }
                    plateLoad = gpu::LoadOp::Load;
            }
            ++stats_.glassPassCount;
            stats_.glassPillCount += static_cast<unsigned>(iso.glassPills.size());
        } else if (iso.backdropSigma > 0.f || iso.backdropBend > 0.f) {
            if (!snapshotted) {
                const int hw = std::max(1, viewportW / 2);
                const int hh = std::max(1, viewportH / 2);
                if (!targets_.backdrop.native() || targets_.backdrop.width() != hw ||
                    targets_.backdrop.height() != hh) {
                    gpu::FrameTargetDesc bgDesc;
                    bgDesc.width = hw;
                    bgDesc.height = hh;
                    bgDesc.mipmaps = true;
                    auto bg = device_.createFrameTarget(bgDesc);
                    if (bg.ok()) {
                        targets_.backdrop = std::move(bg.value());
                    }
                }
                if (targets_.backdrop.native()) {
                    if (!encoder.copyColorTo(targets_.backdrop)) {
                        void* src = nativeColor ? nativeColor : device_.colorNative();
                        if (src) {
                            gpu::PassDesc down;
                            down.nativeColor = targets_.backdrop.native();
                            down.load = gpu::LoadOp::DontCare;
                            down.viewportW = hw;
                            down.viewportH = hh;
                            gpu::Pass dp = encoder.beginPass(down);
                            const Mat4 dproj =
                                Mat4::orthoYDown(0, 0, static_cast<float>(hw), static_cast<float>(hh));
                            setProjection(dp, dproj);
                            BlitInstance blit{};
                            blit.rect[0] = 0.f;
                            blit.rect[1] = 0.f;
                            blit.rect[2] = static_cast<float>(hw);
                            blit.rect[3] = static_cast<float>(hh);
                            blit.uv[2] = 1.f;
                            blit.uv[3] = 1.f;
                            blit.extra[0] = blit.extra[1] = blit.extra[2] = blit.extra[3] = 1.f;
                            dp.setPipeline(pipes_.blit);
                            dp.setBytes(0, &blit, sizeof(blit));
                            dp.setFragmentTexture(0, src);
                            dp.setFragmentSampler(0, device_.nativeSampler());
                            dp.draw(6, 1, 0, 0);
                            dp.end();
                        }
                    }
                    encoder.generateMips(targets_.backdrop);
                }
                snapshotted = true;
            }
            if (targets_.backdrop.native()) {
                gpu::PassDesc plate;
                plate.nativeColor = ft->native();
                plate.load = gpu::LoadOp::Clear;
                plate.clear = {0, 0, 0, 0};
                plate.viewportW = iso.contentW;
                plate.viewportH = iso.contentH;
                gpu::Pass gp = encoder.beginPass(plate);
                const Mat4 localProj =
                    Mat4::orthoYDown(0, 0, iso.destW > 0.f ? iso.destW : static_cast<float>(iso.contentW),
                                     iso.destH > 0.f ? iso.destH : static_cast<float>(iso.contentH));
                setProjection(gp, localProj);
                const float pw = iso.destW > 0.f ? iso.destW : static_cast<float>(iso.contentW);
                const float ph = iso.destH > 0.f ? iso.destH : static_cast<float>(iso.contentH);
                const PlateInstance back = makePlate(
                    0.f, 0.f, pw, ph, iso.backdropU0, iso.backdropV0, iso.backdropU1, iso.backdropV1,
                    iso.backdropSigma, iso.backdropBend, iso.backdropMerge, iso.backdropPress,
                    iso.backdropFlat, iso.backdropLightX, iso.backdropLightY, iso.backdropLightZ,
                    iso.backdropPills.empty() ? nullptr : iso.backdropPills.data(),
                    static_cast<int>(iso.backdropPills.size()));
                gp.setPipeline(pipes_.blur.native() ? pipes_.blur : pipes_.blit);
                gp.setBytes(0, &back, sizeof(back));
                gp.setFragmentBytes(0, &back, sizeof(back));
                gp.setFragmentTexture(0, targets_.backdrop.native());
                gp.setFragmentSampler(0, device_.nativeSampler());
                gp.draw(6, 1, 0, 0);
                gp.end();
                plateLoad = gpu::LoadOp::Load;
            }
        }
        const Mat4 localProj =
            Mat4::orthoYDown(0, 0, iso.destW > 0.f ? iso.destW : static_cast<float>(iso.contentW),
                             iso.destH > 0.f ? iso.destH : static_cast<float>(iso.contentH));
        submitLayer(encoder, iso.quads, iso.blits, iso.gradients, iso.isolates, localProj,
                    iso.contentW, iso.contentH, ft->native(), plateLoad);

        passDesc.load = gpu::LoadOp::Load;
        passDesc.nativeColor = nativeColor;
        pass = encoder.beginPass(passDesc);
        setProjection(pass, projection);
        BlitInstance blit{};
        blit.rect[0] = iso.destX;
        blit.rect[1] = iso.destY;
        blit.rect[2] = iso.destW;
        blit.rect[3] = iso.destH;
        blit.uv[0] = 0.f;
        blit.uv[1] = 0.f;
        blit.uv[2] = isoU1;
        blit.uv[3] = isoV1;
        blit.extra[0] = iso.opacity;
        blit.extra[1] = iso.opacity;
        blit.extra[2] = iso.opacity;
        blit.extra[3] = iso.opacity;
        pass.setPipeline(pipes_.blit);
        pass.setBytes(0, &blit, sizeof(blit));
        pass.setFragmentTexture(0, ft->native());
        pass.setFragmentSampler(0, device_.nativeSampler());
        pass.draw(6, 1, 0, 0);
        stats_.draws += 1;
        stats_.instances += 1;
    }
    pass.end();
}

#if GLIM_EMBED
void Renderer::submit(const FramePacket& packet) {
    const auto t0 = std::chrono::steady_clock::now();
    stats_ = packet.stats;
    if (!pipes_.ensure(device_)) {
        return;
    }
    auto drawable = device_.nextDrawable();
    if (!drawable.ok()) {
        return;
    }
    textures_.setImages(&packet.images);
    targets_.reset();
    for (const BlitQuad& q : packet.blits) {
        textures_.get(q.imageId);
    }
    gpu::CommandEncoder encoder = device_.encoder();
    const Mat4 proj = presentProjection(Mat4::orthoYDown(0, 0, packet.logicalSize.x, packet.logicalSize.y),
                                        device_.presentRotationDegrees());
    submitLayer(encoder, packet.quads, packet.blits, packet.gradients, packet.isolates, proj,
                drawable->width(), drawable->height(), nullptr, gpu::LoadOp::Clear);
    encoder.present(drawable.value());
    encoder.submit(device_.queue());
    stats_.encodeMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
}
#endif

}  // namespace glim::paint
