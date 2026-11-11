#include <metal_stdlib>
using namespace metal;

struct Instance {
  float4 rect;
  float4 uv;
  float4 extra;
};

struct Uniforms {
  float4x4 projection;
};

struct VSOut {
  float4 position [[position]];
  float2 uv;
  float4 tint;
};

vertex VSOut vs_main(
    uint vid [[vertex_id]],
    uint iid [[instance_id]],
    constant Instance* instances [[buffer(0)]],
    constant Uniforms& uniforms [[buffer(1)]]
) {
  const float2 unit[6] = {
      float2(0.0, 0.0),
      float2(1.0, 0.0),
      float2(0.0, 1.0),
      float2(0.0, 1.0),
      float2(1.0, 0.0),
      float2(1.0, 1.0),
  };
  const float2 p = unit[vid];
  const Instance inst = instances[iid];
  VSOut out;
  out.position = uniforms.projection * float4(inst.rect.xy + p * inst.rect.zw, 0.0, 1.0);
  out.uv = mix(inst.uv.xy, inst.uv.zw, p);
  out.tint = inst.extra;
  return out;
}

fragment float4
fs_main(VSOut in [[stage_in]], texture2d<float> tex [[texture(2)]], sampler samp [[sampler(6)]]) {
  // Image bytes are sRGB in a UNORM texture (no hardware decode),
  // so decode manually to the linear working space. Frame targets
  // are sRGB views (hardware decodes on sample) and must use
  // blit_linear instead — never this shader.
  float4 s = tex.sample(samp, in.uv);
  float3 lo = s.rgb / 12.92;
  float3 hi = pow((s.rgb + float3(0.055)) / 1.055, float3(2.4));
  s.rgb = mix(hi, lo, step(s.rgb, float3(0.04045)));
  return s * in.tint;
}
