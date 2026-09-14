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
    const float sigma = in.extra.x;
    const float bend = in.extra.y;
    const float2 texel = in.extra.zw;
    float2 uv = in.uv;
    if (bend > 0.0) {
        const float2 d = uv - float2(0.5, 0.5);
        const float r2 = dot(d, d);
        uv -= d * bend * (0.25 - r2);
    }
    float4 c;
    if (sigma <= 0.5 || texel.x <= 0.0 || texel.y <= 0.0) {
        c = tex.sample(samp, uv);
    } else {
        const float w0 = 0.227027;
        const float w1 = 0.1945946;
        const float w2 = 0.1216216;
        const float w3 = 0.054054;
        const float w4 = 0.016216;
        const float2 step = texel * sigma;
        c = tex.sample(samp, uv) * w0;
        c += tex.sample(samp, uv + float2(step.x, 0.0)) * w1;
        c += tex.sample(samp, uv - float2(step.x, 0.0)) * w1;
        c += tex.sample(samp, uv + float2(0.0, step.y)) * w1;
        c += tex.sample(samp, uv - float2(0.0, step.y)) * w1;
        c += tex.sample(samp, uv + step) * w2;
        c += tex.sample(samp, uv - step) * w2;
        c += tex.sample(samp, uv + float2(step.x, -step.y)) * w2;
        c += tex.sample(samp, uv + float2(-step.x, step.y)) * w2;
        c += tex.sample(samp, uv + 2.0 * float2(step.x, 0.0)) * w3;
        c += tex.sample(samp, uv - 2.0 * float2(step.x, 0.0)) * w3;
        c += tex.sample(samp, uv + 2.0 * float2(0.0, step.y)) * w3;
        c += tex.sample(samp, uv - 2.0 * float2(0.0, step.y)) * w3;
        c += tex.sample(samp, uv + 2.0 * step) * w4;
        c += tex.sample(samp, uv - 2.0 * step) * w4;
    }
    if (bend > 0.0) {
        const float edge = max(abs(in.uv.x - 0.5), abs(in.uv.y - 0.5));
        const float spec = pow(saturate(1.0 - edge * 2.0), 8.0) * bend * 0.55;
        c.rgb += spec;
    }
    return c;
}
