struct Instance {
    rect: vec4<f32>,
    uv: vec4<f32>,
    extra: vec4<f32>,
};

struct Uniforms {
    projection: mat4x4<f32>,
};

@group(0) @binding(0) var<storage, read> instances: array<Instance>;
@group(0) @binding(1) var<storage, read> uniforms: Uniforms;
@group(0) @binding(2) var tex: texture_2d<f32>;
@group(0) @binding(6) var texSampler: sampler;

struct VSOut {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) tint: vec4<f32>,
};

@vertex
fn vs_main(
    @builtin(vertex_index) vid: u32,
    @builtin(instance_index) iid: u32,
) -> VSOut {
    var unit = array<vec2<f32>, 6>(
        vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 0.0), vec2<f32>(0.0, 1.0),
        vec2<f32>(0.0, 1.0), vec2<f32>(1.0, 0.0), vec2<f32>(1.0, 1.0),
    );
    let p = unit[vid];
    let inst = instances[iid];
    var out: VSOut;
    out.position = uniforms.projection * vec4<f32>(inst.rect.xy + p * inst.rect.zw, 0.0, 1.0);
    out.uv = mix(inst.uv.xy, inst.uv.zw, p);
    out.tint = inst.extra;
    return out;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
    let sdf = textureSample(tex, texSampler, in.uv).r;
    let w = max(0.03, fwidth(sdf) * 0.6);
    let a = smoothstep(0.5 - w, 0.5 + w, sdf);
    return vec4<f32>(in.tint.rgb * a, in.tint.a * a);
}
