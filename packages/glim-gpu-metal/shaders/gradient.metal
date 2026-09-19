#include <metal_stdlib>
using namespace metal;

struct Instance {
    float4 rect;      // x, y, w, h (logical pixels)
    float4 grad;      // p0x, p0y, p1x, p1y (logical pixels)
    float4 misc;      // kind (0 linear, 1 radial), radius, coverage, stopCount
    float4 colors[8]; // premultiplied RGBA stops
    float4 off0;      // stop offsets 0..3
    float4 off1;      // stop offsets 4..7
};

struct Uniforms {
    float4x4 projection;
};

struct VSOut {
    float4 position [[position]];
    float2 pos;
    float4 grad;
    float4 misc;
    float4 colors[8];
    float4 off0;
    float4 off1;
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
    const float2 pos = inst.rect.xy + p * inst.rect.zw;
    VSOut out;
    out.position = uniforms.projection * float4(pos, 0.0, 1.0);
    out.pos = pos;
    out.grad = inst.grad;
    out.misc = inst.misc;
    for (uint i = 0; i < 8; ++i) {
        out.colors[i] = inst.colors[i];
    }
    out.off0 = inst.off0;
    out.off1 = inst.off1;
    return out;
}

fragment float4 fs_main(VSOut in [[stage_in]]) {
    const int n = int(in.misc.w + 0.5);
    if (n <= 0) {
        return float4(0.0);
    }
    float t;
    if (in.misc.x < 0.5) {
        const float2 d = in.grad.zw - in.grad.xy;
        const float denom = dot(d, d);
        t = denom > 1e-8 ? dot(in.pos - in.grad.xy, d) / denom : 0.0;
    } else {
        t = in.misc.y > 1e-6 ? length(in.pos - in.grad.xy) / in.misc.y : 0.0;
    }
    t = clamp(t, 0.0, 1.0);
    float off[8] = {in.off0.x, in.off0.y, in.off0.z, in.off0.w,
                    in.off1.x, in.off1.y, in.off1.z, in.off1.w};
    float4 c = in.colors[0];
    for (int i = 1; i < 8; ++i) {
        if (i >= n) {
            break;
        }
        const float o0 = off[i - 1];
        const float o1 = off[i];
        const float f = o1 > o0 ? clamp((t - o0) / (o1 - o0), 0.0, 1.0) : (t >= o1 ? 1.0 : 0.0);
        c = mix(c, in.colors[i], f);
    }
    return c * in.misc.z;
}
