#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_tint;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 2) uniform texture2D tex;
layout(set = 0, binding = 6) uniform sampler samp;

void main() {
    const float sdf = texture(sampler2D(tex, samp), v_uv).r;
    // AA width from the UV scale (texels per pixel x SDF slope), as the CPU
    // path computes it. fwidth(sdf) collapses at a thin stem's peak (the
    // gradient flips sign inside the quad), cutting minified stems away.
    const float perPx = length(dFdx(v_uv * vec2(textureSize(sampler2D(tex, samp), 0))));
    const float w = max(0.03, 0.6 * perPx * (128.0 / 255.0) / 5.0);
    const float a = smoothstep(0.5 - w, 0.5 + w, sdf);
    out_color = vec4(v_tint.rgb * a, v_tint.a * a);
}
