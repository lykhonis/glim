// Glim blit pipeline (WebGPU/WGSL): sampled/foreign pigment on a quad.
// Mirrors the blit vertex layout; fragment samples texture slot 0.
// Slice 2 adds slots 1-3 (blur, luma, lumaPrev) for glass per docs/webgpu.md.

struct BlitInstance {
    rect: vec4<f32>,   // x, y, w, h (logical pixels)
    uv: vec4<f32>,     // u0, v0, u1, v1
    extra: vec4<f32>,  // tint / flags (pipeline-specific)
};

struct Uniforms {
    projection: mat4x4<f32>,
};

@group(0) @binding(0) var<storage, read> instances: array<BlitInstance>;
@group(0) @binding(1) var<uniform> uniforms: Uniforms;
@group(0) @binding(2) var tex: texture_2d<f32>;
@group(0) @binding(3) var texSampler: sampler;

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
    let pos = inst.rect.xy + p * inst.rect.zw;
    var out: VSOut;
    out.position = uniforms.projection * vec4<f32>(pos, 0.0, 1.0);
    out.uv = mix(inst.uv.xy, inst.uv.zw, p);
    out.tint = inst.extra;
    return out;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
    // Premultiplied blit; tint defaults to white (1,1,1,1) when unused.
    return textureSample(tex, texSampler, in.uv) * in.tint;
}
