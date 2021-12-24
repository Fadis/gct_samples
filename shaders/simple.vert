#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 input_position;
layout (location = 0) out vec3 output_color;
out gl_PerVertex
{
    vec4 gl_Position;
};

void main() {
  output_color = vec3(
    min( max( 1.0 - input_position.x - input_position.y, 0.0 ), 1.0 ),
    min( max( input_position.x, 0.0 ), 1.0 ),
    min( max( input_position.y, 0.0 ), 1.0 )
  );
  gl_Position = vec4( input_position, 1.0 );
}

