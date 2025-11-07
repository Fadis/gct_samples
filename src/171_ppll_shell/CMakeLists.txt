add_executable( 171_ppll_shell
  gct.cpp
)
target_compile_definitions( 171_ppll_shell PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_compile_definitions( 171_ppll_shell PRIVATE -DCMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )

target_link_libraries(
  171_ppll_shell
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

add_shader( 171_ppll_shell mix_ao/mix_ao.comp )

add_shader( 171_ppll_shell fur/geometry.task INSTALL )
add_shader( 171_ppll_shell fur/geometry.mesh INSTALL )
add_shader( 171_ppll_shell fur/geometry.frag INSTALL )
