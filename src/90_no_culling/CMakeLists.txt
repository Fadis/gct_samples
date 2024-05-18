add_executable( no_culling
  gct.cpp
)
target_compile_definitions( no_culling PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_compile_definitions( no_culling PRIVATE -DCMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )

add_shader( no_culling shadow_mat/shadow_mat.comp )

add_shader( no_culling shadow/world.frag )
add_shader( no_culling shadow/world.geom )
add_shader( no_culling shadow/world.vert )

add_shader( no_culling skyview/generate_transmittance.comp )
add_shader( no_culling skyview/generate_skyview.comp )
add_shader( no_culling skyview/generate_multiscat.comp )
add_shader( no_culling skyview/froxel.comp )
add_shader( no_culling skyview/render_froxel.comp )

add_shader( no_culling geometry/tangent.frag )
add_shader( no_culling geometry/tangent.vert )
add_shader( no_culling geometry/world.frag )
add_shader( no_culling geometry/world.vert )

add_shader( no_culling geometry_test/world.frag )
add_shader( no_culling geometry_test/world.vert )
add_shader( no_culling geometry_test/world.geom )


add_shader( no_culling matrix_pool/write.comp )
add_shader( no_culling matrix_pool/read.comp )
add_shader( no_culling matrix_pool/update.comp )

add_shader( no_culling aabb_pool/write.comp )
add_shader( no_culling aabb_pool/read.comp )
add_shader( no_culling aabb_pool/update.comp )

add_shader( no_culling primitive_resource_index_pool/write.comp )
add_shader( no_culling primitive_resource_index_pool/read.comp )

add_shader( no_culling instance_resource_index_pool/write.comp )
add_shader( no_culling instance_resource_index_pool/read.comp )

add_shader( no_culling visibility_pool/write.comp )
add_shader( no_culling visibility_pool/read.comp )

add_shader( no_culling gauss/h5_16.comp )
add_shader( no_culling gauss/v5_16.comp )
add_shader( no_culling gauss/h12_32.comp )
add_shader( no_culling gauss/v12_32.comp )

add_shader( no_culling lighting/lighting.comp )
add_shader( no_culling gamma/gamma.comp )

add_shader( no_culling ao/hbao_0.comp )
add_shader( no_culling ao/hbao_45.comp )
add_shader( no_culling ao/hbao_90.comp )
add_shader( no_culling ao/hbao_135.comp )
add_shader( no_culling ao/hbao.comp )

add_shader( no_culling mix_ao/mix_ao.comp )

add_shader( no_culling tone/tone.comp )
