#include <metal_stdlib>
using namespace metal;

struct GlassShape {
    float2 center;
    float2 halfExtent;
    float corner;
    float n;
    float mergeK;
    float _pad;
};

struct GlassUniforms {
    float2 resolution;
    float dpr;
    float shapeCount;
    GlassShape shapes[8];
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
    float4 tint;
    float blurEdge;
    float lumaLift;
    float lumaShadow;
    float lumaOn;
    float dimmer;
    float interactive;
    float flatten;
    float _pad1;
    float2 destUv0;
    float2 destUv1;
};

struct Uniforms {
    float4x4 projection;
};

struct VSOut {
    float4 position [[position]];
    float2 uv;
};

vertex VSOut vs_main(uint vid [[vertex_id]],
                     uint iid [[instance_id]],
                     constant GlassUniforms* instances [[buffer(0)]],
                     constant Uniforms& uniforms [[buffer(1)]]) {
    const float2 unit[6] = {
        float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0),
        float2(0.0, 1.0), float2(1.0, 0.0), float2(1.0, 1.0),
    };
    const float2 p = unit[vid];
    const GlassUniforms inst = instances[iid];
    const float2 logical = inst.resolution / max(inst.dpr, 0.001);
    VSOut out;
    out.position = uniforms.projection * float4(p * logical, 0.0, 1.0);
    out.uv = p;
    return out;
}

float sminCirc(float a, float b, float k) {
    if (k <= 0.001) {
        return min(a, b);
    }
    k *= 1.0 / (1.0 - sqrt(0.5));
    const float h = max(k - abs(a - b), 0.0) / k;
    const float t = max(1.0 - h * (h - 2.0), 0.0);
    return min(a, b) - k * 0.5 * (1.0 + h - sqrt(t));
}

float sdCapsulePx(float2 p, float2 c, float2 he, float r) {
    const float2 a = c - float2(he.x - r, 0.0);
    const float2 b = c + float2(he.x - r, 0.0);
    const float2 pa = p - a;
    const float2 ba = b - a;
    const float h = saturate(dot(pa, ba) / max(dot(ba, ba), 1e-4));
    return length(pa - ba * h) - r;
}

float sdRoundedPx(float2 p, float2 c, float2 he, float r, float n) {
    const float2 q = abs(p - c) - he + r;
    if (q.x > 0.0 && q.y > 0.0 && n > 2.01) {
        const float2 pn = abs(q);
        return pow(pow(pn.x, n) + pow(pn.y, n), 1.0 / n) - r;
    }
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float shapeSdf(thread const GlassUniforms& u, float2 p) {
    const int count = clamp(int(u.shapeCount), 1, 8);
    float d = 1e6;
    for (int i = 0; i < 8; ++i) {
        if (i >= count) {
            break;
        }
        const GlassShape s = u.shapes[i];
        const float2 c = s.center * u.dpr;
        const float2 he = s.halfExtent * u.dpr;
        const float r = min(s.corner * u.dpr, min(he.x, he.y));
        const float k = min(s.mergeK * u.dpr, min(he.x, he.y) * 0.42);
        float sd;
        if (r >= min(he.x, he.y) - 0.5) {
            sd = sdCapsulePx(p, c, he, r);
        } else {
            sd = sdRoundedPx(p, c, he, r, s.n);
        }
        d = (i == 0) ? sd : sminCirc(d, sd, k);
    }
    if (u.interactive > 0.5) {
        d -= 0.03 * min(u.resolution.x, u.resolution.y);
    }
    return d;
}

float safeAsin(float x) {
    return asin(clamp(x, -1.0, 1.0));
}

float rec709(float3 c) {
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

fragment float4 fs_main(VSOut in [[stage_in]],
                        constant GlassUniforms* instances [[buffer(0)]],
                        texture2d<float> u_sharp [[texture(0)]],
                        texture2d<float> u_blur [[texture(1)]],
                        texture2d<float> u_luma [[texture(2)]],
                        texture2d<float> u_lumaPrev [[texture(3)]],
                        sampler samp [[sampler(0)]]) {
    const GlassUniforms u = instances[0];
    const float2 p = in.uv * u.resolution;
    float d = shapeSdf(u, p);
    float2 n2 = float2(dfdx(d), dfdy(d));
    n2 /= max(length(n2), 1e-5);
    const float alphaShape = smoothstep(0.75, -0.75, d);
    if (alphaShape <= 0.001) {
        return float4(0.0);
    }

    if (u.flatten > 0.5) {
        const float fill = 0.94;
        float3 color = float3(fill);
        const float rim = smoothstep(1.6, 0.2, abs(d));
        color = mix(color, float3(1.0), rim * 0.05);
        return float4(color * alphaShape, alphaShape);
    }
    const float T = max(u.thickness * u.dpr, 1.0);
    const float delta = max(-d, 0.0);
    const float wrapT = min(T, 16.0 * max(u.dpr, 1.0));
    float wrapX = 0.0;
    float wrapE = 0.0;
    if (delta < wrapT && u.thickness > 0.001) {
        wrapX = 1.0 - saturate(delta / wrapT);
        const float thetaI = safeAsin(pow(wrapX, 1.25));
        const float eta = max(u.ior, 1.01);
        const float thetaT = safeAsin(sin(thetaI) / eta);
        wrapE = -tan(thetaT - thetaI);
    }
    float2 offset = float2(0.0);
    if (abs(wrapE) > 1e-5) {
        const float magPx = wrapE * (u.refDistance / 0.35) * 28.0 * max(u.dpr, 1.0);
        offset = n2 * magPx / max(u.resolution, float2(1.0));
    }

    const float2 uvIso = clamp(in.uv, float2(0.001), float2(0.999));
    const float2 uvL = mix(u.destUv0, u.destUv1, uvIso);
    const float frost = (u.blurEdge > 0.5) ? 0.88 : 0.32;
    const float3 pane = mix(u_sharp.sample(samp, uvL, level(0.0)).rgb,
                            u_blur.sample(samp, uvIso, level(0.0)).rgb, frost);
    const float gamma = u.dispersion;
    float3 wrapRgb;
    const float2 uvIsoW = clamp(in.uv + offset, float2(0.001), float2(0.999));
    const float2 uvW = mix(u.destUv0, u.destUv1, uvIsoW);
    if (gamma > 0.0 && wrapX > 0.08) {
        const float2 uvIsoR = clamp(in.uv + offset * (1.0 - (0.98 - 1.0) * gamma), float2(0.001), float2(0.999));
        const float2 uvIsoB = clamp(in.uv + offset * (1.0 - (1.02 - 1.0) * gamma), float2(0.001), float2(0.999));
        const float2 uvR = mix(u.destUv0, u.destUv1, uvIsoR);
        const float2 uvB = mix(u.destUv0, u.destUv1, uvIsoB);
        wrapRgb = float3(u_sharp.sample(samp, uvR, level(0.0)).r,
                         u_sharp.sample(samp, uvW, level(0.0)).g,
                         u_sharp.sample(samp, uvB, level(0.0)).b);
    } else {
        wrapRgb = u_sharp.sample(samp, uvW, level(0.0)).rgb;
    }
    wrapRgb = mix(wrapRgb, u_blur.sample(samp, uvIsoW, level(0.0)).rgb, frost);
    const float wrapMix = pow(saturate(wrapX), (u.blurEdge > 0.5) ? 1.25 : 0.95);
    const float3 sampleRgb = mix(pane, wrapRgb, wrapMix);

    float3 color = sampleRgb;
    const float L = rec709(pane);
    const float milkPull = (u.blurEdge > 0.5) ? 0.2 : 0.7;
    const float milk = (u.lumaOn > 0.5 ? 0.22 : 0.08) * (1.0 - wrapX * milkPull);
    color = mix(color, float3(1.0), milk);
    if (u.lumaOn > 0.5) {
        const float high = saturate((L - 0.72) / 0.28);
        const float low = saturate((0.28 - L) / 0.28);
        color *= 1.0 - high * u.lumaShadow;
        color = mix(color, color + float3(0.06, 0.07, 0.08), low * u.lumaLift);
    }
    color = mix(color, u.tint.rgb, u.tint.a * 0.12);
    if (u.dimmer > 0.0 && L > 0.72) {
        color = mix(color, color * 0.82, u.dimmer);
    }

    // Thin inner hairline (peak ~1.2 device px in, past the AA fringe).
    const float dprN = max(u.dpr, 1.0);
    const float inside = max(-d, 0.0);
    const float hair = pow(saturate(1.0 - abs(inside - 1.2 * dprN) / (1.8 * dprN)), 5.0);
    const float horiz = n2.x * n2.x;
    color *= 1.0 - horiz * hair * 0.42;
    const float top = saturate(-n2.y);
    const float bot = saturate(n2.y);
    const float hi = hair * (top * 1.0 + bot * 0.62) * max(u.glareIntensity, 0.7);
    color = mix(color, float3(1.0), saturate(hi));
    if (u.interactive > 0.5) {
        color = mix(color, float3(1.0), top * hair * 0.18);
    }

    return float4(color * alphaShape, alphaShape);
}
