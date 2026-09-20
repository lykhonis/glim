// Glim solid quad pipeline (WebGPU/WGSL).
// Mirrors packages/glim-gpu-metal/shaders/solid.metal.
// Y-down logical pixels; the host ortho matrix flips for the surface.

struct Instance {
    rect: vec4<f32>,   // x, y, w, h (logical pixels)
    color: vec4<f32>,  // premultiplied RGBA
};

struct Uniforms {
    projection: mat4x4<f32>,
};

@group(0) @binding(0) var<storage, read> instances: array<Instance>;
@group(0) @binding(1) var<uniform> uniforms: Uniforms;

struct VSOut {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec4<f32>,
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
    out.color = inst.color;
    return out;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
    return in.color;
}
