add_executable( 163_mark_shell
  gct.cpp
)
target_compile_definitions( 163_mark_shell PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_compile_definitions( 163_mark_shell PRIVATE -DCMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )

target_link_libraries(
  163_mark_shell
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

add_shader( 163_mark_shell shell/geometry.task )
add_shader( 163_mark_shell shell/geometry.mesh )
add_shader( 163_mark_shell shell/geometry.frag )

