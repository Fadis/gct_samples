#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "io.h"
#include "constants.h"
#include "push_constants.h"
#include "lighting.h"

layout(binding = 1) uniform sampler2D base_color;
layout(binding = 4) uniform sampler2D occlusion;

void main()  {
  vec3 normal = normalize( input_normal.xyz );
  vec3 pos = input_position.xyz;
  vec3 N = normal;
  vec3 V = normalize(dynamic_uniforms.eye_pos.xyz-pos);
  vec3 L = normalize(dynamic_uniforms.light_pos.xyz-pos);
  float roughness = uniforms.roughness;
  float metallicness = uniforms.metalness;
  vec4 diffuse_color = texture( base_color, input_texcoord ) * uniforms.base_color;
  float ambient = dynamic_uniforms.ambient * mix( 1 - uniforms.occlusion_strength, 1, texture( occlusion, input_texcoord ).r );
  vec3 emissive = uniforms.emissive.rgb;
  vec3 linear = light( L, V, N, V, N, diffuse_color.rgb, roughness, metallicness, ambient, emissive, dynamic_uniforms.light_energy );
  output_color = vec4( gamma(linear), diffuse_color.a );
}

