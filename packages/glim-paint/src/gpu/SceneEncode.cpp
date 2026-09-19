#include <glim/paint/Renderer.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <glim/assert.h>

#include "../Strip.h"
#include "Isolates.h"

namespace glim::paint {

void Renderer::encodeGroup(gpu::CommandEncoder& encoder, const Group& group, const Mat4& projection,
                           int viewportW, int viewportH, void* nativeColor, gpu::LoadOp load,
                           float pixelRatio, Vec2 logicalSize, const Mat4& extraRoot) {
    gpu::PassDesc passDesc;
    passDesc.nativeColor = nativeColor;
    passDesc.load = load;
    passDesc.clear = {0, 0, 0, nativeColor ? 0.f : 1.f};
    passDesc.viewportW = viewportW;
    passDesc.viewportH = viewportH;
    gpu::Pass pass = encoder.beginPass(passDesc);
    setProjection(pass, projection);

    batch_.clearAll();

    const auto scissorFor = [&](const ClipState& clip) {
        const int vw = std::max(0, viewportW);
        const int vh = std::max(0, viewportH);
        if (!clip.active || vw <= 0 || vh <= 0 || clip.rect.size.x <= 0.f || clip.rect.size.y <= 0.f) {
            pass.setScissor(0, 0, static_cast<std::uint32_t>(vw), static_cast<std::uint32_t>(vh));
            return;
        }
        const float x0 = clip.rect.origin.x;
        const float y0 = clip.rect.origin.y;
        const float x1 = x0 + clip.rect.size.x;
        const float y1 = y0 + clip.rect.size.y;
        const Vec2 corners[4] = {{x0, y0}, {x1, y0}, {x0, y1}, {x1, y1}};
        float minX = 0.f;
        float minY = 0.f;
        float maxX = 0.f;
        float maxY = 0.f;
        for (int i = 0; i < 4; ++i) {
            const Vec4 clipP = projection * Vec4{corners[i].x, corners[i].y, 0.f, 1.f};
            const float iw = clipP.w != 0.f ? 1.f / clipP.w : 1.f;
            const float fx = (clipP.x * iw * 0.5f + 0.5f) * static_cast<float>(vw);
            const float fy = (0.5f - clipP.y * iw * 0.5f) * static_cast<float>(vh);
            if (i == 0) {
                minX = maxX = fx;
                minY = maxY = fy;
            } else {
                minX = std::min(minX, fx);
                minY = std::min(minY, fy);
                maxX = std::max(maxX, fx);
                maxY = std::max(maxY, fy);
            }
        }
        int x = static_cast<int>(std::floor(minX));
        int y = static_cast<int>(std::floor(minY));
        int w = static_cast<int>(std::ceil(maxX)) - x;
        int h = static_cast<int>(std::ceil(maxY)) - y;
        x = std::max(0, std::min(x, vw));
        y = std::max(0, std::min(y, vh));
        w = std::max(0, std::min(w, vw - x));
        h = std::max(0, std::min(h, vh - y));
        pass.setScissor(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y),
                        static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));
    };

    auto addQuad = [this](const Quad& q) {
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
    };
    auto addGradient = [this, &pass](const GradientQuad& q) {
        if (q.w <= 0.f || q.h <= 0.f || q.coverage <= 1e-4f || q.stopCount == 0) {
            return;
        }
        batch_.flushSolid(pass, pipes_, stats_);
        batch_.flushRounded(pass, pipes_, stats_);
        batch_.flushBlit(pass, pipes_, device_.nativeSampler(), stats_);
        batch_.flushGlyph(pass, pipes_, device_.nativeSampler(), stats_);
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
    };
    auto addBlit = [this, &pass](const BlitQuad& q) {
        void* tex = textures_.get(q.imageId);
        if (!tex) {
            return;
        }
        batch_.flushSolid(pass, pipes_, stats_);
        batch_.flushRounded(pass, pipes_, stats_);
        batch_.flushGradient(pass, pipes_, stats_);
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

    bool seenBackdrop = false;
    bool afterBackdrop = false;
    bool seenGlass = false;
    bool afterGlass = false;
    bool snapshotted = false;
    bool glassFrozen = false;

    const auto resumePass = [&]() {
        passDesc.load = gpu::LoadOp::Load;
        passDesc.nativeColor = nativeColor;
        pass = encoder.beginPass(passDesc);
        setProjection(pass, projection);
    };

    const auto freezeGlassSrc = [&]() -> void* {
        const int hw = std::max(1, viewportW / 2);
        const int hh = std::max(1, viewportH / 2);
        if (!targets_.glassSrc.native() || targets_.glassSrc.width() != hw ||
            targets_.glassSrc.height() != hh) {
            auto ft = device_.createFrameTarget({hw, hh});
            if (!ft.ok()) {
                return nullptr;
            }
            targets_.glassSrc = std::move(ft.value());
        }
        if (!nativeColor && encoder.copyColorTo(targets_.glassSrc)) {
            return targets_.glassSrc.native();
        }
        void* src = nativeColor ? nativeColor : device_.colorNative();
        if (!src) {
            return nullptr;
        }
        blitTexture(encoder, pipes_, device_.nativeSampler(), stats_, targets_.glassSrc.native(),
                    hw, hh, src, 0.f, 0.f, static_cast<float>(hw), static_cast<float>(hh), 0.f, 0.f,
                    1.f, 1.f, 1.f, 1.f, 1.f, 1.f);
        return targets_.glassSrc.native();
    };

    const auto emitTree = [&](auto& self, const Group& g, const Mat4& extra,
                             const ClipState& parentClip) -> void {
        const ClipState clip = intersectClip(parentClip, clipOf(g.params, extra));
        batch_.flushSolid(pass, pipes_, stats_);
        batch_.flushRounded(pass, pipes_, stats_);
        batch_.flushGradient(pass, pipes_, stats_);
        batch_.flushBlit(pass, pipes_, device_.nativeSampler(), stats_);
        batch_.flushGlyph(pass, pipes_, device_.nativeSampler(), stats_);
        scissorFor(clip);
        const auto addRounded = [this, &pass, &clip, &addQuad, &addGradient](const Shape& xf) {
            batch_.flushBlit(pass, pipes_, device_.nativeSampler(), stats_);
            batch_.flushGlyph(pass, pipes_, device_.nativeSampler(), stats_);
            batch_.flushGradient(pass, pipes_, stats_);
            if (pipes_.rounded.native()) {
                RoundedInstance inst{};
                const Rect* rect = nullptr;
                const Radius* radius = nullptr;
                const Matter* matter = nullptr;
                float strokeWidth = 0.f;
                if (const auto* r = std::get_if<FillRounded>(&xf)) {
                    rect = &r->rect;
                    radius = &r->radius;
                    matter = &r->matter;
                } else if (const auto* st = std::get_if<Stroke>(&xf)) {
                    rect = &st->rect;
                    radius = &st->radius;
                    matter = &st->matter;
                    strokeWidth = st->width;
                }
                if (!rect || !radius || !matter) {
                    return;
                }
                if (matter->isGradient()) {
                    // Gradient rounded/stroke always rasterizes via strips.
                    std::vector<Quad> unused;
                    std::vector<BlitQuad> unusedBlits;
                    std::vector<GradientQuad> grads;
                    appendShape(unused, unusedBlits, grads, xf, clip);
                    for (const GradientQuad& g : grads) {
                        addGradient(g);
                    }
                    return;
                }
                inst.rect[0] = rect->origin.x;
                inst.rect[1] = rect->origin.y;
                inst.rect[2] = rect->size.x;
                inst.rect[3] = rect->size.y;
                inst.radii[0] = radius->lt;
                inst.radii[1] = radius->rt;
                inst.radii[2] = radius->lb;
                inst.radii[3] = radius->rb;
                const Vec4 premul = matter->color.premul();
                inst.color[0] = premul.x;
                inst.color[1] = premul.y;
                inst.color[2] = premul.z;
                inst.color[3] = premul.w;
                inst.extra[0] = strokeWidth;
                batch_.rounded.push_back(inst);
                return;
            }
            std::vector<Quad> quads;
            std::vector<BlitQuad> unused;
            std::vector<GradientQuad> unusedGrads;
            appendShape(quads, unused, unusedGrads, xf, clip);
            batch_.flushRounded(pass, pipes_, stats_);
            batch_.flushGradient(pass, pipes_, stats_);
            for (const Quad& q : quads) {
                addQuad(q);
            }
        };
        const auto emitShape = [&](const Shape& s) {
            const Shape xf = transformShape(extra, s);
            if (const auto* fill = std::get_if<FillRect>(&xf)) {
                batch_.flushBlit(pass, pipes_, device_.nativeSampler(), stats_);
                batch_.flushGlyph(pass, pipes_, device_.nativeSampler(), stats_);
                batch_.flushRounded(pass, pipes_, stats_);
                batch_.flushGradient(pass, pipes_, stats_);
                if (fill->matter.isGradient()) {
                    std::vector<Quad> unused;
                    std::vector<BlitQuad> unusedBlits;
                    std::vector<GradientQuad> grads;
                    appendShape(unused, unusedBlits, grads, xf, clip);
                    for (const GradientQuad& g : grads) {
                        addGradient(g);
                    }
                    return;
                }
                if (!clip.active) {
                    const Vec4 p = fill->matter.color.premul();
                    Quad q;
                    q.x = fill->rect.origin.x;
                    q.y = fill->rect.origin.y;
                    q.w = fill->rect.size.x;
                    q.h = fill->rect.size.y;
                    q.r = p.x;
                    q.g = p.y;
                    q.b = p.z;
                    q.a = p.w;
                    addQuad(q);
                } else {
                    std::vector<Quad> quads;
                    std::vector<BlitQuad> unused;
                    std::vector<GradientQuad> unusedGrads;
                    appendShape(quads, unused, unusedGrads, xf, clip);
                    for (const Quad& q : quads) {
                        addQuad(q);
                    }
                }
            } else if (std::get_if<FillRounded>(&xf) || std::get_if<Stroke>(&xf)) {
                addRounded(xf);
            } else if (std::get_if<Blit>(&xf) || std::get_if<GlyphRun>(&xf)) {
                std::vector<Quad> unused;
                std::vector<BlitQuad> blits;
                std::vector<GradientQuad> unusedGrads;
                appendShape(unused, blits, unusedGrads, xf, clip);
                for (const BlitQuad& q : blits) {
                    addBlit(q);
                }
            }
        };
        visitGroup(
            g, emitShape,
            [&](const Group& childRef) {
            const Group* child = &childRef;
            const bool backdrop = hasBackdrop(*child);
            const bool glassWork = hasGlassWork(*child);
            if (backdrop) {
                GLIM_ASSERT(!afterBackdrop && !seenGlass,
                            "backdrop Groups must be consecutive ([under*][backdrop*][glass*][overlay*])");
                seenBackdrop = true;
            } else if (glassWork) {
                GLIM_ASSERT(!afterGlass,
                            "glass Groups must be consecutive ([under*][backdrop*][glass*][overlay*])");
                if (seenBackdrop) {
                    afterBackdrop = true;
                }
                seenGlass = true;
            } else if (seenGlass) {
                afterGlass = true;
            } else if (seenBackdrop) {
                afterBackdrop = true;
            }
            if (needsIsolate(*child)) {
                batch_.flushSolid(pass, pipes_, stats_);
                batch_.flushRounded(pass, pipes_, stats_);
                batch_.flushBlit(pass, pipes_, device_.nativeSampler(), stats_);
                batch_.flushGlyph(pass, pipes_, device_.nativeSampler(), stats_);
                pass.end();
                void* backdropTex = nullptr;
                if (backdrop) {
                    if (!snapshotted) {
                        backdropTex = snapshotBackdrop(encoder, pipes_, device_.nativeSampler(),
                                                       device_, targets_, viewportW, viewportH,
                                                       nativeColor, stats_);
                        snapshotted = true;
                        stats_.backdropCount = 1;
                    } else {
                        backdropTex = targets_.backdrop.native();
                    }
                }
                void* glassTex = nullptr;
                if (glassWork) {
                    if (!glassFrozen) {
                        if (freezeGlassSrc()) {
                            glassFrozen = true;
                        }
                    }
                    ++stats_.glassPassCount;
                }
                GLIM_ASSERT(!(backdrop && glassWork), "a Group cannot be both backdrop and glass");
                const Rect surface = glassWork ? glassSurface(*child)
                                               : (backdrop ? backdropSurface(*child)
                                                          : (child->params.bounds.size.x > 0.f &&
                                                                     child->params.bounds.size.y > 0.f
                                                                 ? child->params.bounds
                                                                 : contentBounds(*child)));
                const int iw = isolatePixelSize(surface.size.x, pixelRatio);
                const int ih = isolatePixelSize(surface.size.y, pixelRatio);
                gpu::FrameTarget* ft = targets_.acquire(device_, iw, ih);
                if (ft && ft->native()) {
                    const float isoU1 =
                        static_cast<float>(iw) / static_cast<float>(std::max(1, ft->width()));
                    const float isoV1 =
                        static_cast<float>(ih) / static_cast<float>(std::max(1, ft->height()));
                    auto content = cloneGroup(*child);
                    content->params.opacity = 1.0f;
                    content->params.isolate = false;
                    clearBackdropParams(content->params);
                    content->params.transform = Mat4::identity();
                    if (backdrop) {
                        dropBackdropPills(*content);
                    }
                    if (glassWork) {
                        GlassPill pills[kMaxGlassPills];
                        const int n = collectGlassPills(*child, pills);
                        stats_.glassPillCount += static_cast<unsigned>(n);
                        stripGlassForIsolate(*content);
                    }
                    if (!hasClip(content->params) && surface.size.x > 0.f && surface.size.y > 0.f) {
                        content->params.clip = Rect{{0, 0}, surface.size};
                    }
                    const Mat4 localProj = Mat4::orthoYDown(0, 0, surface.size.x, surface.size.y);
                    // Width-based: isolate content is uniformly scaled by pr
                    // (iw = ceil(w * pr)), so pr == iw/w up to rounding.
                    const float localPr =
                        surface.size.x > 0.f ? static_cast<float>(iw) / surface.size.x : 1.f;
                    if (glassWork) {
                        const Rect dest = transformRect(extra * child->params.transform, surface);
                        float du0 = 0.f;
                        float dv0 = 0.f;
                        float du1 = 1.f;
                        float dv1 = 1.f;
                        if (logicalSize.x > 0.f && logicalSize.y > 0.f) {
                            du0 = dest.origin.x / logicalSize.x;
                            dv0 = dest.origin.y / logicalSize.y;
                            du1 = (dest.origin.x + dest.size.x) / logicalSize.x;
                            dv1 = (dest.origin.y + dest.size.y) / logicalSize.y;
                        }
                        glassTex = targets_.glassSrc.native();
                        Glass mat{};
                        if (child->params.glass.has_value()) {
                            mat = *child->params.glass;
                        } else {
                            for (const auto& gc : child->children) {
                                if (gc && gc->params.glass.has_value()) {
                                    mat = *gc->params.glass;
                                    break;
                                }
                            }
                        }
                        GlassPill gpills[kMaxGlassPills];
                        int gn = collectGlassPills(*child, gpills);
                        for (int i = 0; i < gn; ++i) {
                            gpills[i].rect.origin.x -= surface.origin.x;
                            gpills[i].rect.origin.y -= surface.origin.y;
                        }
                        const GlassUniforms gu =
                            makeGlassUniforms(mat, gpills, gn, iw, ih, localPr, du0, dv0, du1, dv1);
                        void* sharpTex = glassTex;
                        void* blurTex = glassTex;
                        const float blurR = glassBlurRadius(mat);
                        if (sharpTex && blurR > 0.f && pipes_.blur1d.native() && !mat.flatten) {
                            const float sigmaFull = blurR * localPr * 0.5f;
                            blurTex = blurGlass(encoder, pipes_, device_.nativeSampler(), device_,
                                                targets_, sharpTex, iw, ih, du0, dv0, du1, dv1,
                                                sigmaFull, &stats_);
                        }
                        if (sharpTex && pipes_.glass.native() && gn > 0) {
                            gpu::PassDesc plate;
                            plate.nativeColor = ft->native();
                            plate.load = gpu::LoadOp::Clear;
                            plate.clear = {0, 0, 0, 0};
                            plate.viewportW = iw;
                            plate.viewportH = ih;
                            gpu::Pass gp = encoder.beginPass(plate);
                            setProjection(gp, localProj);
                            gp.setPipeline(pipes_.glass);
                            gp.setBytes(0, &gu, sizeof(gu));
                            gp.setFragmentBytes(0, &gu, sizeof(gu));
                            gp.setFragmentTexture(0, sharpTex);
                            gp.setFragmentTexture(1, blurTex ? blurTex : sharpTex);
                            gp.setFragmentSampler(0, device_.nativeSampler());
                            gp.setFragmentSampler(1, device_.nativeSampler());
                            gp.draw(6, 1, 0, 0);
                            stats_.draws += 1;
                            gp.end();
                            glassTex = ft->native();
                        } else if (glassTex) {
                            blitTexture(encoder, pipes_, device_.nativeSampler(), stats_,
                                        ft->native(), iw, ih, glassTex, 0.f, 0.f,
                                        static_cast<float>(iw), static_cast<float>(ih), du0, dv0,
                                        du1, dv1, 1.f, 1.f, 1.f, 1.f);
                        }
                        encodeGroup(encoder, *content, localProj, iw, ih, ft->native(),
                                    glassTex ? gpu::LoadOp::Load : gpu::LoadOp::Clear, localPr,
                                    surface.size, Mat4::translate(-surface.origin.x, -surface.origin.y));
                    } else if (backdrop && backdropTex) {
                        gpu::PassDesc plate;
                        plate.nativeColor = ft->native();
                        plate.load = gpu::LoadOp::Clear;
                        plate.clear = {0, 0, 0, 0};
                        plate.viewportW = iw;
                        plate.viewportH = ih;
                        gpu::Pass gp = encoder.beginPass(plate);
                        setProjection(gp, localProj);
                        const Rect dest = transformRect(extra * child->params.transform, surface);
                        float u0 = 0.f;
                        float v0 = 0.f;
                        float u1 = 1.f;
                        float v1 = 1.f;
                        if (logicalSize.x > 0.f && logicalSize.y > 0.f) {
                            u0 = dest.origin.x / logicalSize.x;
                            v0 = dest.origin.y / logicalSize.y;
                            u1 = (dest.origin.x + dest.size.x) / logicalSize.x;
                            v1 = (dest.origin.y + dest.size.y) / logicalSize.y;
                        }
                        BackdropPill pills[kMaxBackdropPills];
                        const int n = collectBackdropPills(*child, pills);
                        for (int i = 0; i < n; ++i) {
                            pills[i].rect.origin.x -= surface.origin.x;
                            pills[i].rect.origin.y -= surface.origin.y;
                        }
                        const float sigma = snapBackdropSigma(child->params.backdropBlur);
                        const PlateInstance back = makePlate(
                            0.f, 0.f, surface.size.x, surface.size.y, u0, v0, u1, v1, sigma,
                            child->params.backdropBend, child->params.backdropMerge,
                            child->params.backdropPress, child->params.backdropFlat,
                            child->params.backdropLightX, child->params.backdropLightY,
                            child->params.backdropLightZ, pills, n);
                        gp.setPipeline(pipes_.blur.native() ? pipes_.blur : pipes_.blit);
                        gp.setBytes(0, &back, sizeof(back));
                        gp.setFragmentBytes(0, &back, sizeof(back));
                        gp.setFragmentTexture(0, backdropTex);
                        gp.setFragmentSampler(0, device_.nativeSampler());
                        gp.draw(6, 1, 0, 0);
                        stats_.draws += 1;
                        gp.end();
                        encodeGroup(encoder, *content, localProj, iw, ih, ft->native(), gpu::LoadOp::Load,
                                    localPr, surface.size,
                                    Mat4::translate(-surface.origin.x, -surface.origin.y));
                    } else {
                        encodeGroup(encoder, *content, localProj, iw, ih, ft->native(), gpu::LoadOp::Clear,
                                    localPr, surface.size,
                                    Mat4::translate(-surface.origin.x, -surface.origin.y));
                    }
                    ++stats_.isolateCount;
                    resumePass();
                    const Rect dest = transformRect(extra * child->params.transform, surface);
                    BlitInstance blit{};
                    blit.rect[0] = dest.origin.x;
                    blit.rect[1] = dest.origin.y;
                    blit.rect[2] = dest.size.x;
                    blit.rect[3] = dest.size.y;
                    blit.uv[0] = 0.f;
                    blit.uv[1] = 0.f;
                    blit.uv[2] = isoU1;
                    blit.uv[3] = isoV1;
                    blit.extra[0] = child->params.opacity;
                    blit.extra[1] = child->params.opacity;
                    blit.extra[2] = child->params.opacity;
                    blit.extra[3] = child->params.opacity;
                    pass.setPipeline(pipes_.blit);
                    pass.setBytes(0, &blit, sizeof(blit));
                    pass.setFragmentTexture(0, ft->native());
                    pass.setFragmentSampler(0, device_.nativeSampler());
                    pass.draw(6, 1, 0, 0);
                    stats_.draws += 1;
                    stats_.instances += 1;
                } else {
                    resumePass();
                }
                return;
            }
            self(self, *child, extra * child->params.transform, clip);
            });
        batch_.flushSolid(pass, pipes_, stats_);
        batch_.flushRounded(pass, pipes_, stats_);
        batch_.flushGradient(pass, pipes_, stats_);
        batch_.flushBlit(pass, pipes_, device_.nativeSampler(), stats_);
        batch_.flushGlyph(pass, pipes_, device_.nativeSampler(), stats_);
        scissorFor(parentClip);
    };
    emitTree(emitTree, group, extraRoot, {});
    pass.end();
}

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
    textures_.setImages(&scene.images);
    targets_.reset();
    Group root = merge(scene.root, &stats_);
    const auto prefetch = [this](auto& self, const Group& g) -> void {
        for (const Shape& s : g.shapes) {
            if (const auto* b = std::get_if<Blit>(&s)) {
                textures_.get(b->matter.imageId);
            }
        }
        for (const auto& child : g.children) {
            if (child) {
                self(self, *child);
            }
        }
    };
    prefetch(prefetch, root);
    gpu::CommandEncoder encoder = device_.encoder();
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
    const Mat4 proj = presentProjection(Mat4::orthoYDown(0, 0, scene.logicalSize.x, scene.logicalSize.y),
                                        rotation);
    encodeGroup(encoder, root, proj, drawable->width(), drawable->height(), nullptr, gpu::LoadOp::Clear,
                pr, scene.logicalSize);
    encoder.present(drawable.value());
    encoder.submit(device_.queue());
    stats_.encodeMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace glim::paint
