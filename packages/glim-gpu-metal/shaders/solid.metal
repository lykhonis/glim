#include <metal_stdlib>
using namespace metal;

struct Instance {
    float4 rect;   // x, y, w, h (logical pixels)
    float4 color;  // premultiplied RGBA
};

struct Uniforms {
    float4x4 projection;
};

struct VSOut {
    float4 position [[position]];
    float4 color;
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
    out.color = inst.color;
    return out;
}

fragment float4 fs_main(VSOut in [[stage_in]]) {
    return in.color;
}
