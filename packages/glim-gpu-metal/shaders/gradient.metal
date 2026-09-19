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
    uint iid [[flat]];
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
    out.iid = iid;
    return out;
}

fragment float4 fs_main(VSOut in [[stage_in]],
                        constant Instance* instances [[buffer(0)]]) {
    const Instance inst = instances[in.iid];
    const int n = int(inst.misc.w + 0.5);
    if (n <= 0) {
        return float4(0.0);
    }
    float t;
    if (inst.misc.x < 0.5) {
        const float2 d = inst.grad.zw - inst.grad.xy;
        const float denom = dot(d, d);
        t = denom > 1e-8 ? dot(in.pos - inst.grad.xy, d) / denom : 0.0;
    } else {
        t = inst.misc.y > 1e-6 ? length(in.pos - inst.grad.xy) / inst.misc.y : 0.0;
    }
    t = clamp(t, 0.0, 1.0);
    float4 c = inst.colors[0];
    for (int i = 1; i < 8; ++i) {
        if (i >= n) {
            break;
        }
        const float o0 = i <= 4 ? inst.off0[i - 1] : inst.off1[i - 5];
        const float o1 = i < 4 ? inst.off0[i] : inst.off1[i - 4];
        const float f = o1 > o0 ? clamp((t - o0) / (o1 - o0), 0.0, 1.0) : (t >= o1 ? 1.0 : 0.0);
        c = mix(c, inst.colors[i], f);
    }
    return c * inst.misc.z;
}
