#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// 三角形プリミティブ1つを入力として受け取る
layout (triangles) in;
// 最大で3頂点1プリミティブを出力する
layout (triangle_strip,max_vertices=3) out;

in gl_PerVertex
{
    vec4 gl_Position;
} gl_in[];

out gl_PerVertex
{
    vec4 gl_Position;
};

void main() {
  // 頂点のスクリーン座標系での座標をセットする
  gl_Position = vec4( 0, 0, 0, 1 );
  EmitVertex();
  gl_Position = vec4( 0, 1, 0, 1 );
  EmitVertex();
  gl_Position = vec4( 1, 0, 0, 1 );
  EmitVertex();
  // 上でセットした3つの頂点で1つの三角形にする
  EndPrimitive();
}

