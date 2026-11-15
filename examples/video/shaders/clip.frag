#version 450
#extension GL_GOOGLE_include_directive : require

#define float2 vec2
#define float3 vec3
#define CONST const
vec3 saturate(vec3 c) { return clamp(c, 0.0, 1.0); }

#include "clip.scene"

layout(push_constant) uniform Clip {
  vec2 size;
  float t;
} clip;

layout(location = 0) out vec4 color;

void main() {
  color = vec4(shade(gl_FragCoord.xy / clip.size, clip.t, floor(gl_FragCoord.xy)), 1.0);
}
