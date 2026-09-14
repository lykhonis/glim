#include <metal_stdlib>
using namespace metal;

struct Instance {
    float4 rect;
    float4 radii;
    float4 color;
    float4 extra;
};

struct Uniforms {
    float4x4 projection;
};

struct VSOut {
    float4 position [[position]];
    float2 logical;
    uint iid [[flat]];
};

float coverageAt(float2 p, float4 rect, float4 radii) {
    const float2 center = rect.xy + 0.5 * rect.zw;
    const float2 b = 0.5 * rect.zw;
    const float2 q = p - center;
    float r = 0.0;
    if (q.x < 0.0 && q.y < 0.0) {
        r = radii.x;
    } else if (q.x >= 0.0 && q.y < 0.0) {
        r = radii.y;
    } else if (q.x < 0.0 && q.y >= 0.0) {
        r = radii.z;
    } else {
        r = radii.w;
    }
    const float2 d = abs(q) - b + float2(r, r);
    const float outside = length(max(d, float2(0.0))) + min(max(d.x, d.y), 0.0) - r;
    return saturate(0.5 - outside);
}

float coverage(float2 p, Instance inst) {
    const float strokeWidth = inst.extra.x;
    if (strokeWidth > 0.0) {
        const float halfw = strokeWidth * 0.5;
        const float4 outer = float4(inst.rect.xy - halfw, inst.rect.zw + strokeWidth);
        const float4 orad = inst.radii + halfw;
        const float outerCov = coverageAt(p, outer, orad);
        const float2 innerSize = inst.rect.zw - strokeWidth;
        if (innerSize.x > 0.0 && innerSize.y > 0.0) {
            const float4 inner = float4(inst.rect.xy + halfw, innerSize);
            const float4 irad = max(inst.radii - halfw, float4(0.0));
            return max(0.0, outerCov - coverageAt(p, inner, irad));
        }
        return outerCov;
    }
    return coverageAt(p, inst.rect, inst.radii);
}

vertex VSOut vs_main(uint vid [[vertex_id]],
                     uint iid [[instance_id]],
                     constant Instance* instances [[buffer(0)]],
                     constant Uniforms& uniforms [[buffer(1)]]) {
    const float2 unit[6] = {
        float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0),
        float2(0.0, 1.0), float2(1.0, 0.0), float2(1.0, 1.0),
    };
    const Instance inst = instances[iid];
    const float pad = inst.extra.x * 0.5 + 1.0;
    const float2 origin = inst.rect.xy - pad;
    const float2 size = inst.rect.zw + 2.0 * pad;
    const float2 pos = origin + unit[vid] * size;
    VSOut out;
    out.position = uniforms.projection * float4(pos, 0.0, 1.0);
    out.logical = pos;
    out.iid = iid;
    return out;
}

fragment float4 fs_main(VSOut in [[stage_in]],
                        constant Instance* instances [[buffer(0)]]) {
    const Instance inst = instances[in.iid];
    return inst.color * coverage(in.logical, inst);
}
