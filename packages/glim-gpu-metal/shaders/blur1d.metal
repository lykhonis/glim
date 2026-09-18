#include <metal_stdlib>
using namespace metal;

struct Instance {
    float4 rect;
    float4 uv;
    float4 extra;
};

struct Uniforms {
    float4x4 projection;
};

struct VSOut {
    float4 position [[position]];
    float2 uv;
    float4 extra;
};

vertex VSOut vs_main(uint vid [[vertex_id]],
                     uint iid [[instance_id]],
                     constant Instance* instances [[buffer(0)]],
                     constant Uniforms& uniforms [[buffer(1)]]) {
    const float2 unit[6] = {
        float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0),
        float2(0.0, 1.0), float2(1.0, 0.0), float2(1.0, 1.0),
    };
    const float2 p = unit[vid];
    const Instance inst = instances[iid];
    VSOut out;
    out.position = uniforms.projection * float4(inst.rect.xy + p * inst.rect.zw, 0.0, 1.0);
    out.uv = mix(inst.uv.xy, inst.uv.zw, p);
    out.extra = inst.extra;
    return out;
}

fragment float4 fs_main(VSOut in [[stage_in]],
                         texture2d<float> tex [[texture(0)]],
                         sampler samp [[sampler(0)]]) {
    const float sigma = max(in.extra.x, 0.001);
    // Capped kernel: Renderer downsamples large sigmas so sigma here stays small.
    const int radius = clamp(int(ceil(3.0 * sigma)), 1, 12);
    const float2 texel = float2(in.extra.y / max(float(tex.get_width()), 1.0),
                                in.extra.z / max(float(tex.get_height()), 1.0));
    float4 acc = float4(0.0);
    float wt = 0.0;
    for (int k = -12; k <= 12; ++k) {
        if (abs(k) > radius) {
            continue;
        }
        const float d = float(k) / sigma;
        const float w = exp(-0.5 * d * d);
        acc += tex.sample(samp, in.uv + texel * float(k), level(0.0)) * w;
        wt += w;
    }
    return acc / max(wt, 1e-5);
}
