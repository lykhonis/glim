#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_extra;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 2) uniform sampler2D tex;

void main() {
    const float sigma = max(v_extra.x, 0.001);
    const int radius = clamp(int(ceil(3.0 * sigma)), 1, 8);
    const ivec2 ts = textureSize(tex, 0);
    const vec2 texel = vec2(v_extra.y / max(float(ts.x), 1.0), v_extra.z / max(float(ts.y), 1.0));
    vec4 acc = vec4(0.0);
    float wt = 0.0;
    for (int k = -8; k <= 8; ++k) {
        if (abs(k) > radius) {
            continue;
        }
        const float d = float(k) / sigma;
        const float w = exp(-0.5 * d * d);
        acc += textureLod(tex, v_uv + texel * float(k), 0.0) * w;
        wt += w;
    }
    out_color = acc / max(wt, 1e-5);
}
