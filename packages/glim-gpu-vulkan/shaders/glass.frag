#version 450

struct GlassShape {
    vec2 center;
    vec2 halfExtent;
    float corner, n, mergeK, _pad;
};

struct GlassUniforms {
    vec2 resolution;
    float dpr;
    float shapeCount;
    GlassShape shapes[8];
    float thickness, ior, refDistance, dispersion;
    float fresnelRange, fresnelHardness, fresnelIntensity, glareAngle;
    float glareRange, glareHardness, glareConvergence, glareIntensity;
    vec4 tint;
    float blurEdge, lumaLift, lumaShadow, lumaOn;
    float dimmer, interactive, flatten, _pad1;
    vec2 lightDir;
    vec2 _pad2;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances {
    GlassUniforms instances[];
};

layout(set = 0, binding = 2) uniform sampler2D u_sharp;
layout(set = 0, binding = 3) uniform sampler2D u_blur;
layout(set = 0, binding = 4) uniform sampler2D u_luma;
layout(set = 0, binding = 5) uniform sampler2D u_lumaPrev;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 out_color;

const float kPi = 3.14159265359;

float sminCirc(float a, float b, float k) {
    if (k <= 0.001) {
        return min(a, b);
    }
    k *= 1.0 / (1.0 - sqrt(0.5));
    const float h = max(k - abs(a - b), 0.0) / k;
    const float t = max(1.0 - h * (h - 2.0), 0.0);
    return min(a, b) - k * 0.5 * (1.0 + h - sqrt(t));
}

float sdCapsulePx(vec2 p, vec2 c, vec2 he, float r) {
    const vec2 a = c - vec2(he.x - r, 0.0);
    const vec2 b = c + vec2(he.x - r, 0.0);
    const vec2 pa = p - a;
    const vec2 ba = b - a;
    const float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-4), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

float sdRoundedPx(vec2 p, vec2 c, vec2 he, float r, float n) {
    const vec2 q = abs(p - c) - he + r;
    if (q.x > 0.0 && q.y > 0.0 && n > 2.01) {
        const vec2 pn = abs(q);
        return pow(pow(pn.x, n) + pow(pn.y, n), 1.0 / n) - r;
    }
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float shapeSdf(GlassUniforms u, vec2 p) {
    const int count = clamp(int(u.shapeCount), 1, 8);
    float d = 1e6;
    for (int i = 0; i < 8; ++i) {
        if (i >= count) {
            break;
        }
        const GlassShape s = u.shapes[i];
        const vec2 c = s.center * u.dpr;
        const vec2 he = s.halfExtent * u.dpr;
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

float rec709(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    const GlassUniforms u = instances[0];
    const vec2 p = v_uv * u.resolution;
    float d = shapeSdf(u, p);
    const float alphaShape = smoothstep(0.75, -0.75, d);
    if (alphaShape <= 0.001) {
        out_color = vec4(0.0);
        return;
    }

    if (u.flatten > 0.5) {
        vec3 color = vec3(0.94);
        const float rim = smoothstep(1.6, 0.2, abs(d));
        color = mix(color, vec3(1.0), rim * 0.05);
        out_color = vec4(color * alphaShape, alphaShape);
        return;
    }

    const vec2 e = vec2(1.0, 0.0);
    vec2 n2 = vec2(shapeSdf(u, p + e.xy) - shapeSdf(u, p - e.xy),
                   shapeSdf(u, p + e.yx) - shapeSdf(u, p - e.yx));
    const float nlen = max(length(n2), 1e-5);
    n2 /= nlen;
    const float T = max(u.thickness * u.dpr, 1.0);
    const float delta = max(-d, 0.0);
    const float wrapT = min(T, 16.0 * max(u.dpr, 1.0));
    float wrapX = 0.0;
    float wrapE = 0.0;
    if (delta < wrapT && u.thickness > 0.001) {
        wrapX = 1.0 - clamp(delta / wrapT, 0.0, 1.0);
        const float thetaI = safeAsin(pow(wrapX, 1.25));
        const float eta = max(u.ior, 1.01);
        const float thetaT = safeAsin(sin(thetaI) / eta);
        wrapE = -tan(thetaT - thetaI);
    }
    vec2 offset = vec2(0.0);
    if (abs(wrapE) > 1e-5) {
        const float magPx = wrapE * (u.refDistance / 0.35) * 28.0 * max(u.dpr, 1.0);
        offset = n2 * magPx / max(u.resolution, vec2(1.0));
    }

    const vec2 uvL = clamp(v_uv, vec2(0.001), vec2(0.999));
    const float frost = (u.blurEdge > 0.5) ? 0.88 : 0.32;
    const vec3 pane = mix(texture(u_sharp, uvL).rgb, texture(u_blur, uvL).rgb, frost);
    const float gamma = u.dispersion;
    vec3 wrapRgb;
    const vec2 uvW = clamp(v_uv + offset, vec2(0.001), vec2(0.999));
    if (gamma > 0.0 && wrapX > 0.08) {
        const vec2 uvR = clamp(v_uv + offset * (1.0 - (0.98 - 1.0) * gamma), vec2(0.001), vec2(0.999));
        const vec2 uvB = clamp(v_uv + offset * (1.0 - (1.02 - 1.0) * gamma), vec2(0.001), vec2(0.999));
        wrapRgb = vec3(texture(u_sharp, uvR).r, texture(u_sharp, uvW).g, texture(u_sharp, uvB).b);
    } else {
        wrapRgb = texture(u_sharp, uvW).rgb;
    }
    wrapRgb = mix(wrapRgb, texture(u_blur, uvW).rgb, frost);
    const float wrapMix = pow(clamp(wrapX, 0.0, 1.0), (u.blurEdge > 0.5) ? 1.25 : 0.95);
    const vec3 sampleRgb = mix(pane, wrapRgb, wrapMix);

    vec3 color = sampleRgb;
    const float L = rec709(pane);
    const float milkPull = (u.blurEdge > 0.5) ? 0.2 : 0.7;
    const float milk = (u.lumaOn > 0.5 ? 0.22 : 0.08) * (1.0 - wrapX * milkPull);
    color = mix(color, vec3(1.0), milk);
    if (u.lumaOn > 0.5) {
        const float high = clamp((L - 0.72) / 0.28, 0.0, 1.0);
        const float low = clamp((0.28 - L) / 0.28, 0.0, 1.0);
        color *= 1.0 - high * u.lumaShadow;
        color = mix(color, color + vec3(0.06, 0.07, 0.08), low * u.lumaLift);
    }
    color = mix(color, u.tint.rgb, u.tint.a * 0.12);
    if (u.dimmer > 0.0 && L > 0.72) {
        color = mix(color, color * 0.82, u.dimmer);
    }

    const float dprN = max(u.dpr, 1.0);
    const float inside = max(-d, 0.0);
    const float hair = pow(clamp(1.0 - abs(inside - 1.2 * dprN) / (1.8 * dprN), 0.0, 1.0), 5.0);
    const float horiz = n2.x * n2.x;
    color *= 1.0 - horiz * hair * 0.42;
    const float top = clamp(-n2.y, 0.0, 1.0);
    const float bot = clamp(n2.y, 0.0, 1.0);
    const float hi = hair * (top * 1.0 + bot * 0.62) * max(u.glareIntensity, 0.7);
    color = mix(color, vec3(1.0), clamp(hi, 0.0, 1.0));
    if (u.interactive > 0.5) {
        color = mix(color, vec3(1.0), top * hair * 0.18);
    }

    out_color = vec4(color * alphaShape, alphaShape);
}
