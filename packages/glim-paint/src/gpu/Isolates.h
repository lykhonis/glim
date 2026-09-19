#pragma once

#include <glim/paint/Renderer.h>

namespace glim::paint {

struct Uniforms {
    float projection[16];
};

struct PlateInstance {
    float rect[4];
    float uv[4];
    float extra[4];
    float light[4];
    float pill0[4];
    float pill1[4];
    float pill2[4];
    float pill3[4];
    float radii[4];
};

struct GlassShapeU {
    float center[2];
    float halfExtent[2];
    float corner;
    float n;
    float mergeK;
    float _pad;
};

struct GlassUniforms {
    float resolution[2];
    float dpr;
    float shapeCount;
    GlassShapeU shapes[8];
    float thickness;
    float ior;
    float refDistance;
    float dispersion;
    float fresnelRange;
    float fresnelHardness;
    float fresnelIntensity;
    float glareAngle;
    float glareRange;
    float glareHardness;
    float glareConvergence;
    float glareIntensity;
    float tint[4];
    float blurEdge;
    float lumaLift;
    float lumaShadow;
    float lumaOn;
    float dimmer;
    float interactive;
    float flatten;
    float _pad1;
    float destUv0[2];
    float destUv1[2];
};

static_assert(sizeof(GlassUniforms) == 384, "GlassUniforms packed std430");

void setProjection(gpu::Pass& pass, const Mat4& m);
Mat4 presentProjection(const Mat4& ortho, int degrees);
float glassBlurRadius(const Glass& g);
GlassUniforms makeGlassUniforms(const Glass& g, const GlassPill* pills, int n, int iw, int ih,
                                float pixelRatio, float u0 = 0.f, float v0 = 0.f, float u1 = 1.f,
                                float v1 = 1.f);
PlateInstance makePlate(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                        float sigma, float bend, float mergeK, float press, bool flat, float lx,
                        float ly, float lz, const BackdropPill* pills, int n);
void blitTexture(gpu::CommandEncoder& encoder, const Renderer::Pipelines& pipes, void* sampler,
                 Stats& stats, void* dst, int dw, int dh, void* src, float x, float y, float w,
                 float h, float u0, float v0, float u1, float v1, float r, float g, float b,
                 float a);
void* blurGlass(gpu::CommandEncoder& encoder, const Renderer::Pipelines& pipes, void* sampler,
                gpu::Device& device, Renderer::TargetPool& targets, void* sharp, int contentW,
                int contentH, float su0, float sv0, float su1, float sv1, float sigmaFull,
                Stats* stats);
void* snapshotBackdrop(gpu::CommandEncoder& encoder, const Renderer::Pipelines& pipes,
                       void* sampler, gpu::Device& device, Renderer::TargetPool& targets,
                       int viewportW, int viewportH, void* nativeColor, Stats& stats);

}  // namespace glim::paint
