#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "io_with_tangent.h"
#include "constants.h"
#include "push_constants.h"
#include "lighting.h"
#include "shadow.h"

layout(binding = 3) uniform sampler2D normal_map;

void main()  {
  vec3 normal = normalize( input_normal.xyz );
  vec3 tangent = normalize( input_tangent.xyz );
  vec3 binormal = cross( tangent, normal );
  mat3 ts = transpose( mat3( tangent, binormal, normal ) );
  vec3 pos = input_position.xyz;
  vec3 N = normalize( texture( normal_map, input_texcoord ).rgb * vec3( uniforms.normal_scale, uniforms.normal_scale, 1 ) * 2.0 - 1.0 );
  vec3 V = ts * normalize(dynamic_uniforms.eye_pos.xyz-pos);
  vec3 L = ts * normalize(dynamic_uniforms.light_pos.xyz-pos);
  float roughness = uniforms.roughness;
  float metallicness = uniforms.metalness;
  vec4 diffuse_color = uniforms.base_color;
  float ambient = dynamic_uniforms.ambient;
  vec3 emissive = uniforms.emissive.rgb;
  float sh = shadow( input_shadow0, input_shadow1, input_shadow2, input_shadow3 );
  vec3 WV = normalize(dynamic_uniforms.eye_pos.xyz-pos);
  vec3 WN = normal;
  vec3 linear = light_with_mask( L, V, N, WV, WN, diffuse_color.rgb, roughness, metallicness, ambient, emissive, dynamic_uniforms.light_energy, sh );
  output_color = vec4( gamma(linear), diffuse_color.a );
}

