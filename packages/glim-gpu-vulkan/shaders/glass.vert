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

layout(std140, set = 0, binding = 1) uniform Uniforms {
    mat4 projection;
};

layout(location = 0) out vec2 v_uv;

void main() {
    const vec2 unit[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
        vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));
    const vec2 p = unit[gl_VertexIndex];
    const GlassUniforms inst = instances[gl_InstanceIndex];
    const vec2 logical = inst.resolution / max(inst.dpr, 0.001);
    gl_Position = projection * vec4(p * logical, 0.0, 1.0);
    v_uv = p;
}
