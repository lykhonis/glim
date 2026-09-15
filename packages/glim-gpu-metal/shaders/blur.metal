#include <metal_stdlib>
using namespace metal;

struct Instance {
    float4 rect;
    float4 uv;
    float4 extra;
    float4 light;
    float4 pill0;
    float4 pill1;
    float4 pill2;
    float4 pill3;
    float4 radii;
};

struct Uniforms {
    float4x4 projection;
};

struct VSOut {
    float4 position [[position]];
    float2 uv;
    float2 local;
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
    VSOut out;
    out.position = uniforms.projection * float4(inst.rect.xy + p * inst.rect.zw, 0.0, 1.0);
    out.uv = mix(inst.uv.xy, inst.uv.zw, p);
    out.local = p;
    out.iid = iid;
    return out;
}

float sdRoundBox(float2 p, float2 b, float r) {
    r = min(r, min(b.x, b.y));
    const float2 q = abs(p) - b + float2(r, r);
    return length(max(q, float2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

float smin(float a, float b, float k) {
    if (k <= 0.001) {
        return min(a, b);
    }
    const float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * k * 0.25;
}

float4 pillAt(Instance inst, int i) {
    if (i == 1) {
        return inst.pill1;
    }
    if (i == 2) {
        return inst.pill2;
    }
    if (i == 3) {
        return inst.pill3;
    }
    return inst.pill0;
}

float radiusAt(Instance inst, int i) {
    if (i == 1) {
        return inst.radii.y;
    }
    if (i == 2) {
        return inst.radii.z;
    }
    if (i == 3) {
        return inst.radii.w;
    }
    return inst.radii.x;
}

float fieldSdf(Instance inst, float2 p) {
    const int count = max(1, int(inst.extra.w) & 7);
    const float k = inst.extra.z;
    float sdf = 1e6;
    for (int i = 0; i < 4; ++i) {
        if (i >= count) {
            break;
        }
        const float4 pill = pillAt(inst, i);
        const float2 c = pill.xy + 0.5 * pill.zw;
        const float d = sdRoundBox(p - c, 0.5 * pill.zw, radiusAt(inst, i));
        sdf = (i == 0) ? d : smin(sdf, d, k);
    }
    sdf += inst.light.w * 0.035 * min(inst.rect.z, inst.rect.w);
    return sdf;
}

float3 sampleBlur(texture2d<float> tex, sampler samp, float2 uv, float sigma, float2 texel) {
    uv = saturate(uv);
    const float span = mix(1.0, 2.6, saturate(sigma / 12.0));
    const float2 s = texel * span;
    const float w[5] = {0.0625, 0.25, 0.375, 0.25, 0.0625};
    float3 c = float3(0.0);
    for (int j = 0; j < 5; ++j) {
        const float y = float(j - 2) * s.y;
        for (int i = 0; i < 5; ++i) {
            const float2 p = uv + float2(float(i - 2) * s.x, y);
            c += tex.sample(samp, saturate(p), level(0.0)).rgb * (w[i] * w[j]);
        }
    }
    return c;
}

fragment float4 fs_main(VSOut in [[stage_in]],
                        constant Instance* instances [[buffer(0)]],
                        texture2d<float> tex [[texture(0)]],
                        sampler samp [[sampler(0)]]) {
    const Instance inst = instances[in.iid];
    const float sigma = inst.extra.x;
    const float bend = inst.extra.y;
    const bool flatten = ((int(inst.extra.w) >> 3) & 1) != 0;
    const float2 size = inst.rect.zw;
    const float2 p = in.local * size;
    const float sdf = fieldSdf(inst, p);
    const float aa = max(fwidth(sdf), 1e-3);
    const float mask = saturate(-sdf / aa);
    if (mask < 0.001) {
        return float4(0.0);
    }
    const float minSide = max(1.0, min(size.x, size.y));
    const float2 texel = float2(1.0 / float(max(uint(1), tex.get_width())),
                                1.0 / float(max(uint(1), tex.get_height())));
    const float2 uvScale = (inst.uv.zw - inst.uv.xy) / max(size, float2(1.0));
    const float thickness = max(18.0, minSide * 0.42);
    const float height = saturate(-sdf / thickness);
    const float2 g = float2(dfdx(sdf), dfdy(sdf));
    const float2 n2 = g / max(length(g), 1e-4);
    const float edge = 1.0 - height;
    const float mag = pow(edge, 4.0) * bend * min(48.0, minSide * 0.30);

    float2 uv = saturate(in.uv);
    if (bend > 0.0 && !flatten) {
        uv = saturate(uv + n2 * mag * uvScale);
    }
    float3 col = sampleBlur(tex, samp, uv, sigma, texel);
    if (flatten) {
        const float lum = dot(col, float3(0.2126, 0.7152, 0.0722));
        col = mix(float3(0.18), float3(0.92), step(0.45, lum));
    } else {
        const float frost = mix(0.06, 0.36, saturate(sigma / 12.0));
        col = mix(col, float3(1.0), frost);
        if (bend > 0.0) {
            const float3 N = normalize(float3(n2 * edge * 1.05, 1.0));
            const float3 L = normalize(float3(-0.32, 0.8, 0.48));
            const float spec = pow(saturate(dot(N, L)), 72.0) * pow(edge, 2.4) * 0.85;
            col += spec;
        }
    }
    return float4(col * mask, mask);
}
