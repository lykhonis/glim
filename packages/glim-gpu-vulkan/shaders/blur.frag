#version 450

struct Instance {
    vec4 rect;
    vec4 uv;
    vec4 extra;
    vec4 light;
    vec4 pill0;
    vec4 pill1;
    vec4 pill2;
    vec4 pill3;
    vec4 radii;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances {
    Instance instances[];
};

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec2 v_local;
layout(location = 2) flat in int v_iid;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 2) uniform sampler2D tex;

float sdSquircle(vec2 p, vec2 b, float r) {
    r = min(r, min(b.x, b.y));
    const vec2 q = abs(p) - b + vec2(r);
    const vec2 m = max(q, vec2(0.0));
    return pow(pow(m.x, 4.0) + pow(m.y, 4.0), 0.25) + min(max(q.x, q.y), 0.0) - r;
}

float smin(float a, float b, float k) {
    if (k <= 0.001) {
        return min(a, b);
    }
    const float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * k * 0.25;
}

vec4 pillAt(Instance inst, int i) {
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

float fieldSdf(Instance inst, vec2 p) {
    const int count = max(1, int(inst.extra.w) & 7);
    const float k = inst.extra.z;
    float sdf = 1e6;
    for (int i = 0; i < 4; ++i) {
        if (i >= count) {
            break;
        }
        const vec4 pill = pillAt(inst, i);
        const vec2 c = pill.xy + 0.5 * pill.zw;
        const float d = sdSquircle(p - c, 0.5 * pill.zw, radiusAt(inst, i));
        sdf = (i == 0) ? d : smin(sdf, d, k);
    }
    sdf += inst.light.w * 0.035 * min(inst.rect.z, inst.rect.w);
    return sdf;
}

vec3 sampleBlur(vec2 uv, float sigma, vec2 texel) {
    uv = clamp(uv, 0.0, 1.0);
    const float span = mix(1.0, 2.6, clamp(sigma / 12.0, 0.0, 1.0));
    const vec2 s = texel * span;
    const float w[5] = float[](0.0625, 0.25, 0.375, 0.25, 0.0625);
    vec3 c = vec3(0.0);
    for (int j = 0; j < 5; ++j) {
        const float y = float(j - 2) * s.y;
        for (int i = 0; i < 5; ++i) {
            const vec2 p = uv + vec2(float(i - 2) * s.x, y);
            c += texture(tex, clamp(p, 0.0, 1.0)).rgb * (w[i] * w[j]);
        }
    }
    return c;
}

void main() {
    const Instance inst = instances[v_iid];
    const float sigma = inst.extra.x;
    const float bend = inst.extra.y;
    const bool flatten = ((int(inst.extra.w) >> 3) & 1) != 0;
    const vec2 size = inst.rect.zw;
    const vec2 p = v_local * size;
    const float sdf = fieldSdf(inst, p);
    const float aa = max(fwidth(sdf), 1e-3);
    const float mask = clamp(-sdf / aa, 0.0, 1.0);
    if (mask < 0.001) {
        out_color = vec4(0.0);
        return;
    }
    const float minSide = max(1.0, min(size.x, size.y));
    const vec2 texel = 1.0 / vec2(textureSize(tex, 0));
    const vec2 uvScale = (inst.uv.zw - inst.uv.xy) / max(size, vec2(1.0));
    const float thickness = max(18.0, minSide * 0.42);
    const float height = clamp(-sdf / thickness, 0.0, 1.0);
    const vec2 g = vec2(dFdx(sdf), dFdy(sdf));
    const vec2 n2 = g / max(length(g), 1e-4);
    const float edge = 1.0 - height;
    const float mag = pow(edge, 4.0) * bend * min(48.0, minSide * 0.30);

    vec2 uv = clamp(v_uv, 0.0, 1.0);
    if (bend > 0.0 && !flatten) {
        uv = clamp(uv + n2 * mag * uvScale, 0.0, 1.0);
    }
    vec3 col = sampleBlur(uv, sigma, texel);
    if (flatten) {
        const float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
        col = mix(vec3(0.18), vec3(0.92), step(0.45, lum));
    } else {
        const float frost = mix(0.06, 0.36, clamp(sigma / 12.0, 0.0, 1.0));
        col = mix(col, vec3(1.0), frost);
        if (bend > 0.0) {
            const vec3 N = normalize(vec3(n2 * edge * 1.05, 1.0));
            const vec3 L = normalize(vec3(-0.32, 0.8, 0.48));
            const float spec = pow(clamp(dot(N, L), 0.0, 1.0), 72.0) * pow(edge, 2.4) * 0.85;
            col += spec;
        }
    }
    out_color = vec4(col * mask, mask);
}
