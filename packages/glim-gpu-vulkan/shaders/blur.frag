#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_extra;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 2) uniform sampler2D tex;

void main() {
    const float sigma = v_extra.x;
    const float bend = v_extra.y;
    const vec2 texel = v_extra.zw;
    vec2 uv = v_uv;
    if (bend > 0.0) {
        const vec2 d = uv - vec2(0.5);
        const float r2 = dot(d, d);
        uv -= d * bend * (0.25 - r2);
    }
    vec4 c;
    if (sigma <= 0.5 || texel.x <= 0.0 || texel.y <= 0.0) {
        c = texture(tex, uv);
    } else {
        const float w0 = 0.227027;
        const float w1 = 0.1945946;
        const float w2 = 0.1216216;
        const float w3 = 0.054054;
        const float w4 = 0.016216;
        const vec2 stepv = texel * sigma;
        c = texture(tex, uv) * w0;
        c += texture(tex, uv + vec2(stepv.x, 0.0)) * w1;
        c += texture(tex, uv - vec2(stepv.x, 0.0)) * w1;
        c += texture(tex, uv + vec2(0.0, stepv.y)) * w1;
        c += texture(tex, uv - vec2(0.0, stepv.y)) * w1;
        c += texture(tex, uv + stepv) * w2;
        c += texture(tex, uv - stepv) * w2;
        c += texture(tex, uv + vec2(stepv.x, -stepv.y)) * w2;
        c += texture(tex, uv + vec2(-stepv.x, stepv.y)) * w2;
        c += texture(tex, uv + 2.0 * vec2(stepv.x, 0.0)) * w3;
        c += texture(tex, uv - 2.0 * vec2(stepv.x, 0.0)) * w3;
        c += texture(tex, uv + 2.0 * vec2(0.0, stepv.y)) * w3;
        c += texture(tex, uv - 2.0 * vec2(0.0, stepv.y)) * w3;
        c += texture(tex, uv + 2.0 * stepv) * w4;
        c += texture(tex, uv - 2.0 * stepv) * w4;
    }
    if (bend > 0.0) {
        const float edge = max(abs(v_uv.x - 0.5), abs(v_uv.y - 0.5));
        const float spec = pow(clamp(1.0 - edge * 2.0, 0.0, 1.0), 8.0) * bend * 0.55;
        c.rgb += spec;
    }
    out_color = c;
}
