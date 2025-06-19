add_executable( gauss_dof_with_k+buffer
  gct.cpp
)
target_compile_definitions( gauss_dof_with_k+buffer PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_compile_definitions( gauss_dof_with_k+buffer PRIVATE -DCMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )

target_link_libraries(
  gauss_dof_with_k+buffer
  gct
  gct_glfw
  Threads::Threads
  Boost::program_options
  Boost::system
  ${pkgcfg_lib_Vulkan_vulkan}
  ${OIIO_LIBRARIES}
  ${pkgcfg_lib_GLFW_glfw}
  ${pkgcfg_lib_GLFW_glfw3}
)

add_shader( gauss_dof_with_k+buffer shadow_mat/shadow_mat.comp )



add_shader( gauss_dof_with_k+buffer shadow/world.frag )
add_shader( gauss_dof_with_k+buffer shadow/world.geom )
add_shader( gauss_dof_with_k+buffer shadow/world.vert )

add_shader( gauss_dof_with_k+buffer skyview/generate_transmittance.comp )
add_shader( gauss_dof_with_k+buffer skyview/generate_skyview.comp )
add_shader( gauss_dof_with_k+buffer skyview/generate_multiscat.comp )
add_shader( gauss_dof_with_k+buffer skyview/froxel.comp )
add_shader( gauss_dof_with_k+buffer skyview/render_froxel.comp )

add_shader( gauss_dof_with_k+buffer geometry/tangent.frag )
add_shader( gauss_dof_with_k+buffer geometry/tangent.vert )
add_shader( gauss_dof_with_k+buffer geometry/world.frag )
add_shader( gauss_dof_with_k+buffer geometry/world.vert )
add_shader( gauss_dof_with_k+buffer geometry/dummy.comp )

add_shader( gauss_dof_with_k+buffer aabb/world.frag )
add_shader( gauss_dof_with_k+buffer aabb/world.geom )
add_shader( gauss_dof_with_k+buffer aabb/world.vert )

add_shader( gauss_dof_with_k+buffer depth/world.frag )
add_shader( gauss_dof_with_k+buffer depth/world.vert )

add_shader( gauss_dof_with_k+buffer matrix_pool/write.comp )
add_shader( gauss_dof_with_k+buffer matrix_pool/read.comp )
add_shader( gauss_dof_with_k+buffer matrix_pool/update.comp )
add_shader( gauss_dof_with_k+buffer matrix_pool/copy.comp )

add_shader( gauss_dof_with_k+buffer aabb_pool/write.comp )
add_shader( gauss_dof_with_k+buffer aabb_pool/read.comp )
add_shader( gauss_dof_with_k+buffer aabb_pool/update.comp )

add_shader( gauss_dof_with_k+buffer image_pool/rgba8.comp )
add_shader( gauss_dof_with_k+buffer image_pool/rgba16.comp )
add_shader( gauss_dof_with_k+buffer image_pool/rgba16f.comp )
add_shader( gauss_dof_with_k+buffer image_pool/rgba32f.comp )

add_shader( gauss_dof_with_k+buffer primitive_resource_index_pool/write.comp )
add_shader( gauss_dof_with_k+buffer primitive_resource_index_pool/read.comp )

add_shader( gauss_dof_with_k+buffer instance_resource_index_pool/write.comp )
add_shader( gauss_dof_with_k+buffer instance_resource_index_pool/read.comp )

add_shader( gauss_dof_with_k+buffer visibility_pool/write.comp )
add_shader( gauss_dof_with_k+buffer visibility_pool/read.comp )

add_shader( gauss_dof_with_k+buffer resource_pair/write.comp )
add_shader( gauss_dof_with_k+buffer resource_pair/read.comp )

add_shader( gauss_dof_with_k+buffer light_pool/write.comp )
add_shader( gauss_dof_with_k+buffer light_pool/read.comp )
add_shader( gauss_dof_with_k+buffer light_pool/update.comp )

add_shader( gauss_dof_with_k+buffer gauss/h5_16.comp )
add_shader( gauss_dof_with_k+buffer gauss/v5_16.comp )
add_shader( gauss_dof_with_k+buffer gauss/h12_32.comp )
add_shader( gauss_dof_with_k+buffer gauss/v12_32.comp )

add_shader( gauss_dof_with_k+buffer lighting/lighting.comp )
add_shader( gauss_dof_with_k+buffer gamma/gamma.comp )

add_shader( gauss_dof_with_k+buffer ao/hbao_0.comp )
add_shader( gauss_dof_with_k+buffer ao/hbao_45.comp )
add_shader( gauss_dof_with_k+buffer ao/hbao_90.comp )
add_shader( gauss_dof_with_k+buffer ao/hbao_135.comp )
add_shader( gauss_dof_with_k+buffer ao/hbao.comp )

add_shader( gauss_dof_with_k+buffer mix_ao/mix_ao.comp )

add_shader( gauss_dof_with_k+buffer tone/tone.comp )


add_shader( gauss_dof_with_k+buffer distance/distance.comp )

add_shader( gauss_dof_with_k+buffer dof/hadaptive.comp )
add_shader( gauss_dof_with_k+buffer dof/vadaptive.comp )

add_shader( gauss_dof_with_k+buffer merge/merge.comp )

add_shader( gauss_dof_with_k+buffer coc_gauss/h12_32.comp )
add_shader( gauss_dof_with_k+buffer coc_gauss/v12_32.comp )
