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
    float4 tint;
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
    out.tint = inst.extra;
    return out;
}

fragment float4 fs_main(VSOut in [[stage_in]],
                        texture2d<float> tex [[texture(0)]],
                        sampler samp [[sampler(0)]]) {
    const float sdf = tex.sample(samp, in.uv).r;
    const float w = max(0.03, fwidth(sdf) * 0.6);
    const float a = smoothstep(0.5 - w, 0.5 + w, sdf);
    return float4(in.tint.rgb * a, in.tint.a * a);
}
